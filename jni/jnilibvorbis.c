#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include <time.h>
#include <math.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <android/log.h>

#define READ 1024
signed char readbuffer[READ*4+44]; /* out of the data segment, not the stack */

ogg_stream_state os; /* take physical pages, weld into a logical
                      stream of packets */
ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
ogg_packet       op; /* one raw packet of data for decode */

vorbis_info      vi; /* struct that stores all the static vorbis bitstream settings */	// for encoder
vorbis_info      *vii; /* struct that stores all the static vorbis bitstream settings */	// for decoder
vorbis_comment   vc; /* struct that stores all the user comments */

vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
vorbis_block     vb; /* local working space for packet->PCM decode */

//FILEポインタ作成
FILE *finp;
FILE *foutp;
int size = 0;

OggVorbis_File vf;
int eof=0;
int current_section;
char *ov_buf = 0;

typedef struct {
    char *buf;		//メモリ上のogg vorbis
    int siz;			//サイズ
    int pos;			//現在位置
} OVMEM;
OVMEM *pom;


// モノラル→ステレオ変換
// 引数
//  jbyteArray jinput(I)  モノラルデータ 
//  jbyteArray joutput(O) ステレオデータ用バッファ
//  int        jsize(I)   モノラルデータのサイズ
//
// 戻り値
//  int ステレオデータのサイズ
//
int Java_my_kotatsu_ndk_1practice5_Record_convertMONOtoSteJNI( JNIEnv* env, jobject thiz, jbyteArray jinput, jbyteArray joutput, jint jsize){
    
	int i;
    
	jbyte *input  =  (*env)->GetByteArrayElements(env,jinput,0);
	jbyte *output  = (*env)->GetByteArrayElements(env,joutput,0);
    
    //★データ変換
    memcpy( output, input, jsize);
    
	(*env)->ReleaseByteArrayElements(env,jinput,input,0);
	(*env)->ReleaseByteArrayElements(env,joutput,output,0);
    
	return jsize;
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertPCMToOggJNIStart( JNIEnv* env, jobject thiz, jstring foutpath, jint sampling, jfloat quality){
    int eos=0,ret;
    int i, founddata;
    //キャスト
    const char *foutchar = (*env)->GetStringUTFChars(env,foutpath,0);
    //FILEポインタ作成
    foutp = fopen(foutchar,"wb");
    //	  finp = fopen("/sdcard/wazapp/test.pcm","wb");
    //__android_log_print(ANDROID_LOG_INFO,"RS","1001 %ld¥n",(int)foutp);
    //メモリ開放
    (*env)->ReleaseStringUTFChars(env, foutpath, foutchar);
    //__android_log_print(ANDROID_LOG_INFO,"RS","1002 %s¥n",foutchar);
    
    
    /* we cheat on the WAV header; we just bypass the header and never
     verify that it matches 16bit/stereo/44.1kHz.  This is just an
     example, after all. */
    
    readbuffer[0] = '¥0';
    
    /********** Encode setup ************/
    vorbis_info_init(&vi);
    
    //__android_log_print(ANDROID_LOG_INFO,"RS","1003");
    //エンコーディングの質の設定
    ret=vorbis_encode_init_vbr(&vi,2,sampling,quality);
    /* do not continue if setup failed; this can happen if we ask for a
     mode that libVorbis does not support (eg, too low a bitrate, etc,
     will return 'OV_EIMPL') */
    
    if(ret){
        return(1);
    }
    
    //__android_log_print(ANDROID_LOG_INFO,"RS","1004");
    /* add a comment */
    vorbis_comment_init(&vc);
    vorbis_comment_add_tag(&vc,"ENCODER","libvorbisencoder");
    //__android_log_print(ANDROID_LOG_INFO,"RS","1005");
    
    /* set up the analysis state and auxiliary encoding storage */
    vorbis_analysis_init(&vd,&vi);
    vorbis_block_init(&vd,&vb);
    //__android_log_print(ANDROID_LOG_INFO,"RS","1006");
    
    /* set up our packet->stream encoder */
    /* pick a random serial number; that way we can more likely build
     chained streams just by concatenation */
    srand(time(NULL));
    ogg_stream_init(&os,rand());
    //__android_log_print(ANDROID_LOG_INFO,"RS","1006");
    
    /* Vorbis streams begin with three headers; the initial header (with
     most of the codec setup parameters) which is mandated by the Ogg
     bitstream spec.  The second header holds any comment fields.  The
     third header holds the bitstream codebook.  We merely need to
     make the headers, then pass them to libvorbis one at a time;
     libvorbis handles the additional Ogg bitstream constraints */
    
    {
	    ogg_packet header;
	    ogg_packet header_comm;
	    ogg_packet header_code;
        
	    vorbis_analysis_headerout(&vd,&vc,&header,&header_comm,&header_code);
	    ogg_stream_packetin(&os,&header); /* automatically placed in its own
                                           page */
        //__android_log_print(ANDROID_LOG_INFO,"RS","1008");
	    ogg_stream_packetin(&os,&header_comm);
	    ogg_stream_packetin(&os,&header_code);
        //__android_log_print(ANDROID_LOG_INFO,"RS","1009");
        
	    /* This ensures the actual
	     * audio data will start on a new page, as per spec
	     */
	    while(!eos){
            int result=ogg_stream_flush(&os,&og);
            if(result==0)break;
            fwrite(og.header,1,og.header_len,foutp);
            fwrite(og.body,1,og.body_len,foutp);
	    }
        
        //		  //__android_log_print(ANDROID_LOG_INFO,"RS","1003 %ld¥n",(int)finp);
        //__android_log_print(ANDROID_LOG_INFO,"RS","1004 %ld¥n",(int)foutp);
    }
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertPCMToOggJNIUpdate( JNIEnv* env, jobject thiz,jbyteArray buf,jint size,jint final){
    int eos=0,ret;
    long rp = 0;
    jbyte *rbuf  = (*env)->GetByteArrayElements(env,buf,0);
    //	  //__android_log_print(ANDROID_LOG_INFO,"RS","2001 %ld %ld %ld %ld %ld¥n",suzuki++,eos,final,rp,size);
    //	  fwrite(rbuf,1,size,finp);
    //	  //__android_log_print(ANDROID_LOG_INFO,"RS","20011¥n");
    while( (!eos && final) || ( rp != size && !final ) ){
	    long i;
        //	    long bytes=fread(readbuffer,1,READ*4,finp); /* stereo hardwired here */
	    long bytes = READ*4;
	    if( size <= rp ){
	    	bytes = 0;
	    }else if( bytes > size - rp ){
	    	bytes = size - rp;
	    }
        //		  //__android_log_print(ANDROID_LOG_INFO,"RS","200111¥n");
        //		  //__android_log_print(ANDROID_LOG_INFO,"RS","200112 %ld %ld %ld %ld¥n",bytes,rp,size,(int)rbuf);
	    memcpy(readbuffer,rbuf+rp,bytes);
	    rp += bytes;
        //		  //__android_log_print(ANDROID_LOG_INFO,"RS","20012¥n");
        
        //		  __android_log_print(ANDROID_LOG_INFO,"RS","2002 %ld %ld %ld¥n",bytes,rp,size);
	    if(bytes==0 && final == 1){
            //		 __android_log_print(ANDROID_LOG_INFO,"RS","20021¥n");
            //		if(size==0){
            /* end of file.  this can be done implicitly in the mainline,
	         but it's easier to see here in non-clever fashion.
	         Tell the library we're at end of stream so that it can handle
	         the last frame and mark end of stream in the output properly */
            vorbis_analysis_wrote(&vd,0);
	    }else{
            /* data to encode */
            /* expose the buffer to submit data */
            float **buffer=vorbis_analysis_buffer(&vd,READ);
            //			 __android_log_print(ANDROID_LOG_INFO,"RS","20022¥n");
            
            /* uninterleave samples */
            for(i=0;i<bytes/4;i++){
                buffer[0][i]=((readbuffer[i*4+1]<<8)|
                              (0x00ff&(int)readbuffer[i*4]))/32768.f;
                buffer[1][i]=((readbuffer[i*4+3]<<8)|
                              (0x00ff&(int)readbuffer[i*4+2]))/32768.f;
            }
            
            /* tell the library how much we actually submitted */
            vorbis_analysis_wrote(&vd,i);
	    }
        
        //	    __android_log_print(ANDROID_LOG_INFO,"RS","2003 ¥n");
	    /* vorbis does some data preanalysis, then divvies up blocks for
         more involved (potentially parallel) processing.  Get a single
         block for encoding now */
	    while(vorbis_analysis_blockout(&vd,&vb)==1){
            //		    __android_log_print(ANDROID_LOG_INFO,"RS","20031 ¥n");
            
            /* analysis, assume we want to use bitrate management */
            vorbis_analysis(&vb,NULL);
            vorbis_bitrate_addblock(&vb);
            
            while(vorbis_bitrate_flushpacket(&vd,&op)){
                //	  	    __android_log_print(ANDROID_LOG_INFO,"RS","20032 ¥n");
                
                /* weld the packet into the bitstream */
                ogg_stream_packetin(&os,&op);
                
                /* write out pages (if any) */
                while(!eos){
                    int result=ogg_stream_pageout(&os,&og);
                    //	    	    __android_log_print(ANDROID_LOG_INFO,"RS","20033 ¥n");
                    if(result==0)break;
                    fwrite(og.header,1,og.header_len,foutp);
                    fwrite(og.body,1,og.body_len,foutp);
                    fflush(foutp);
                    
                    /* this could be set above, but for illustrative purposes, I do
                     it here (to show that vorbis does know where the stream ends) */
                    
                    if(ogg_page_eos(&og))eos=1;
                }
            }
	    }
    }
    (*env)->ReleaseByteArrayElements(env,buf,rbuf,0);
    //	  __android_log_print(ANDROID_LOG_INFO,"RS","2004 ¥n");
    
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertPCMToOggJNIEnd( JNIEnv* env, jobject thiz){
    /* clean up and exit.  vorbis_info_clear() must be called last */
    
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    //	 fclose(finp);
    fclose(foutp);
    
    /* ogg_page and ogg_packet structs always point to storage in
     libvorbis.  They're never freed or manipulated directly */
    
    //	  fprintf(stderr,"Done.¥n");
    return(0);
}

size_t mread(char *ptr, size_t size, size_t nmemb, OVMEM *pom)
{
	//__android_log_print(ANDROID_LOG_INFO,"RS","mread¥n");
    if (pom == NULL) return -1;
    if ((pom->pos >= pom->siz) || (pom->pos == -1)) return 0;
    if (pom->pos + size*nmemb >= pom->siz) {
        int ret;
        memcpy(ptr, &pom->buf[pom->pos], pom->siz - pom->pos);
        ret = (pom->siz - pom->pos)/size;
        pom->pos = pom->siz;
        return ret;
    }
    memcpy(ptr, &pom->buf[pom->pos], nmemb * size);
    pom->pos += (nmemb * size);
    return nmemb;
}

int mseek(OVMEM *pom, ogg_int64_t offset, int whence)
{
    int newpos;
	//__android_log_print(ANDROID_LOG_INFO,"RS","mseek¥n");
    
    if (pom == NULL || pom->pos < 0) return -1;
    if (offset < 0) {
        pom->pos = -1;
        return -1;
    }
    switch (whence) {
        case SEEK_SET:
            newpos = offset;
            break;
        case SEEK_CUR:
            newpos = pom->pos + offset;
            break;
        case SEEK_END:
            newpos = pom->siz + offset;
            break;
        default:
            return -1;
    }
    if (newpos < 0) return -1;
    pom->pos = newpos;
    
    return 0;
}

long mtell(OVMEM *pom)
{
	//__android_log_print(ANDROID_LOG_INFO,"RS","mtell¥n");
    if (pom == NULL) return -1;
    return pom->pos;
}

int mclose(OVMEM *pom)
{
	//__android_log_print(ANDROID_LOG_INFO,"RS","mclose¥n");
    if (pom == NULL) return -1;
    free(pom->buf);
    free(pom);
    return 0;
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIStart( JNIEnv* env, jobject thiz, jstring finpath){
    
    int ret = 0;
    const char *finchar  = (*env)->GetStringUTFChars(env,finpath,0);
    //	  const char *foutchar = (*env)->GetStringUTFChars(env,foutpath,0);
    finp  = fopen(finchar, "rb");
    //	  foutp = fopen(foutchar,"wb");
    //メモリ開放
    (*env)->ReleaseStringUTFChars(env, finpath, finchar);
    //	  (*env)->ReleaseStringUTFChars(env, foutpath, foutchar);
    
    ret = ov_open(finp, &vf, NULL, 0);
    if (ret < 0 ) {
        //__android_log_print(ANDROID_LOG_INFO,"RS","ov_open error¥n");
        return -1;
    }
    vii = ov_info(&vf,-1);
    //__android_log_print(ANDROID_LOG_INFO,"RS","ogg info %ld %ld %ld¥n",vii->rate, vii->channels, (int)ov_time_total(&vf, -1));
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIStartofMemory( JNIEnv* env, jobject thiz, jbyteArray buf, jint size){
    
    int ret = 0;
    jbyte *rbuf  = (*env)->GetByteArrayElements(env,buf,0);
    ov_buf = malloc(size);
    memcpy(ov_buf,rbuf,size);
    ov_callbacks oc;
    
    //コールバック関数を指定
    oc.read_func = &mread;
    oc.seek_func = &mseek;
    oc.close_func = &mclose;
    oc.tell_func = &mtell;
    
    pom = malloc(sizeof(OVMEM));
    pom->pos = 0;
    pom->siz = size;
    pom->buf = ov_buf;
    
    ret = ov_open_callbacks(pom, &vf, NULL, 0, oc);
    if (ret < 0 ) {
        //__android_log_print(ANDROID_LOG_INFO,"RS","ov_open error¥n");
        return -1;
    }
    vii = ov_info(&vf,-1);
    //__android_log_print(ANDROID_LOG_INFO,"RS","ogg info %ld %ld %ld¥n",vii->rate, vii->channels, (int)ov_time_total(&vf, -1));
    (*env)->ReleaseByteArrayElements(env,buf,rbuf,0);
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetCH( JNIEnv* env, jobject thiz){
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetCH ¥n");
	return vii->channels;
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetFS( JNIEnv* env, jobject thiz){
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetFS ¥n");
	return vii->rate;
}

double
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetTotalTime( JNIEnv* env, jobject thiz){
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetTotalTime ¥n");
	return (double)ov_time_total(&vf, -1);
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetTotalPCMSize( JNIEnv* env, jobject thiz){
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetTotalPCMSize ¥n");
	return (int)ov_pcm_total(&vf, -1);
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNISetSeekTime( JNIEnv* env, jobject thiz,jdouble sp){
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNISetSeekTime ¥n");
	return (int)ov_time_seek_lap(&vf, sp);
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNISetSeekPCM( JNIEnv* env, jobject thiz,int pos){
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNISetSeekPCM ¥n");
	return (int)ov_pcm_seek_lap(&vf, pos);
}

double
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetPlayingTime (JNIEnv* env, jobject thiz){
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetPlayingTime ¥n");
	return (double)ov_time_tell(&vf);
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetPlayingPCM(JNIEnv* env, jobject thiz){
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIGetPlayingPCM ¥n");
	return (int)ov_pcm_tell(&vf);
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIUpdate( JNIEnv* env, jobject thiz,jbyteArray buf ,jint buf_size){
    jbyte *rbuf  = (*env)->GetByteArrayElements(env,buf,0);
    int read = 0;
    int read_size = 0;
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIUpdate ¥n");
    while(read_size + read < buf_size){
        //__android_log_print(ANDROID_LOG_INFO,"RS","jni001 %d¥n",buf_size);
        read=ov_read(&vf,rbuf+read_size,buf_size - read_size,0,2,1,&current_section);
        if(!read){
            break;
        }
        read_size += read;
        //__android_log_print(ANDROID_LOG_INFO,"RS","jni002 %ld %ld %ld¥n",read, current_section, (int)rbuf);
    }
    //     fwrite(rbuf,1,read_size,foutp);
    //__android_log_print(ANDROID_LOG_INFO,"RS","jni003¥n");
    (*env)->ReleaseByteArrayElements(env,buf,rbuf,0);
    //__android_log_print(ANDROID_LOG_INFO,"RS","jni004¥n");
    return read_size;
}

int
Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIEnd( JNIEnv* env, jobject thiz){
    //__android_log_print(ANDROID_LOG_INFO,"RS","Java_my_kotatsu_ndk_1practice5_Record_convertOggToPCMJNIEnd ¥n");
    ov_clear(&vf);
    if( finp != 0 ){
    	fclose(finp);
    };
    //    if( ov_buf != 0 ){
    //    	free(ov_buf);
    //    }
    //    fclose(foutp);
}
