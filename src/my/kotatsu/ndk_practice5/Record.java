package my.kotatsu.ndk_practice5;


import java.io.File;
import java.io.FileInputStream;
import android.app.Activity;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.view.View;
import android.view.View.*;
import android.widget.EditText;
import android.widget.TextView;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.util.Log;
import android.media.AudioManager;
import android.media.AudioTrack;

public class Record extends Activity implements OnClickListener {

	private String outputFilePath;
	private final int maxDurationInMs = 15000;
	private final String userid = "record";
	private final String outputFileExtension = ".ogg";
	private final String dirName = "/DCIM/";
	private long epoch;
	private AudioRecord record;
	private final int AUDIO_FS = 16000;
	//private final int AUDIO_FS = 44100;
	private int AUDIO_BS_MONO;
	private int AUDIO_BS_STE;
	private final int VORBIS_QUALITY = 0;
	private View button;
	private EditText quality_input;
	private TextView debug,button_tx;
	private long record_time = 0;

	private byte[][] audioBuffer = new byte[30][];
	private int audioBuffer_flg = 0;
	private int audioBuffer_encflg = 0;
	private int enc_flg = 0;
	private int buf_len;
	private int pcm_len[] = new int[30];
	private int play_pcm_len[] = new int[30];
	private int buf_out_fin = 0;
	private int wr_buf_cnt = 0;
	private int rd_buf_cnt = 0;
	private boolean play_flg = false;
	private int mode;
	private final int MODE_REC=1;
	private final int MODE_STOP=2;
	private final int MODE_PLAY=3;
	private byte[] tmpBuffer;
	Handler handler= new Handler();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        AUDIO_BS_MONO = AudioRecord.getMinBufferSize(AUDIO_FS, AudioFormat.CHANNEL_CONFIGURATION_MONO, AudioFormat.ENCODING_PCM_16BIT);
//    	AUDIO_BS_STE  = AudioRecord.getMinBufferSize(AUDIO_FS, AudioFormat.CHANNEL_CONFIGURATION_STEREO, AudioFormat.ENCODING_PCM_16BIT);
        AUDIO_BS_STE = AUDIO_BS_MONO*2; 
        
        //録音の準備
        try{
        	record = new AudioRecord(MediaRecorder.AudioSource.MIC,AUDIO_FS,AudioFormat.CHANNEL_CONFIGURATION_MONO,AudioFormat.ENCODING_PCM_16BIT,AUDIO_BS_MONO);
        }catch(IllegalArgumentException e){
        	e.printStackTrace();
        }
        	
        	
        button = findViewById(R.id.button);
        button_tx = (TextView)(findViewById(R.id.button));
        button_tx.setText("REC");
        mode=MODE_REC;

        debug = (TextView)(findViewById(R.id.textView2));
        debug.setText("");

        button.setOnClickListener(this);
    }

	@Override
	public void onPause(){
		super.onPause();

	}

	@Override
	public void onClick(View v){
    	switch(v.getId()){
    	case R.id.button:
            if( mode == MODE_REC ){
            	button_tx.setText("STOP");
            	mode = MODE_STOP;
            	startRecording();
            }
            else if( mode == MODE_STOP ){
            	button_tx.setText("PLAY");
            	mode = MODE_PLAY;
            	stopRecording();
            }
            else if( mode == MODE_PLAY ){
            	button.setEnabled(false);
            	mode = MODE_REC;
            	playSound();
            }
    		break;
    	}
    }

	//----------------------------------//
	// 録音開始
	//----------------------------------//
	private void startRecording(){		
    	epoch = System.currentTimeMillis();
    	outputFilePath = Environment.getExternalStorageDirectory().getAbsolutePath() +dirName + userid + epoch + outputFileExtension;
    	record_time = System.currentTimeMillis();
    	record.startRecording();
    	debug.setText("start recording.");
    	tmpBuffer = new byte[AUDIO_BS_MONO];
    	for(int i = 0 ; i < 30 ; i++ ){
    		audioBuffer[i] = new byte[AUDIO_BS_STE];
    	}

    	//----------------------------------//
    	//録音データ読み込みスレッド
    	//----------------------------------//
    	new Thread(new Runnable(){
    		@Override
    		public void run(){
    			//Ogg初期化
    	    	convertPCMToOggJNIStart(outputFilePath , AUDIO_FS, VORBIS_QUALITY);

    	    	//録音終了までループしてバッファにPCMデータをためこむ
    	    	while(record.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING){
    	    		//バッファが空いていたら録音データ読み込み
    	    		if(audioBuffer_encflg+30 > audioBuffer_flg){
						pcm_len[audioBuffer_flg%30] = record.read(tmpBuffer,0,AUDIO_BS_MONO);
						//モノステ変換 16bitモノラル → 16bitステレオ
						pcm_len[audioBuffer_flg%30] = convertMONOtoSteJNI( tmpBuffer, audioBuffer[audioBuffer_flg%30],pcm_len[audioBuffer_flg%30]);

						if( pcm_len[audioBuffer_flg%30] == 0 ){
							break;
						}
						audioBuffer_flg++;
    	    		}
    				try{
    					Thread.sleep(10);
    				} catch (InterruptedException ie){
    				}

    			}
    		}
    	}).start();

    	//----------------------------------//
    	// PCM → Ogg変換＆ファイル書き込みスレッド
    	//----------------------------------//
    	new Thread(new Runnable(){
    		@Override
    		public void run(){

    			//初回のバッファ読み込みを待つ
				while(audioBuffer_flg == 0){
    				try{
    					Thread.sleep(10);
    				} catch (InterruptedException ie){
    				}
				}

				// バッファのPCMをOggに変換し、ファイルに書き込み(JNI内で実施)
				while(true){
					//バッファ読み込み済みの場合
					if( (audioBuffer_flg-1) > audioBuffer_encflg){
						convertPCMToOggJNIUpdate(audioBuffer[audioBuffer_encflg%30],pcm_len[audioBuffer_encflg%30],0);
						audioBuffer_encflg++;
					//録音終了時
					}else if( (audioBuffer_flg-1) == audioBuffer_encflg && record.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING){
						convertPCMToOggJNIUpdate(audioBuffer[audioBuffer_encflg%30],pcm_len[audioBuffer_encflg%30],1);
						audioBuffer_encflg++;
						break;
					}
    				try{
    					Thread.sleep(10);
    				} catch (InterruptedException ie){
    				}
				}
    	    	convertPCMToOggJNIEnd();
        		audioBuffer_flg = 0;
    	    	audioBuffer_encflg = 0;
    		}
    	}).start();
    }

	//----------------------------------//
	// 録音終了
	//----------------------------------//
    private void stopRecording(){
    	record.stop();
    	record_time = System.currentTimeMillis() - record_time;
    	debug.setText("stopping recording.recording time = "+record_time+"ms");
    }

	//----------------------------------//
	// 再生
	//----------------------------------//
    private void playSound(){

		debug.setText("Now Playing");
    	for(int i = 0 ; i < 30 ; i++ ){
    		pcm_len[i] = 1;
    	}
    	for(int i = 0 ; i < 30 ; i++ ){
    		audioBuffer[i] = null;
    	}
        wr_buf_cnt = 0;
        rd_buf_cnt = 0;
        buf_out_fin = 0;

    	play_flg = true;
    	//----------------------------------//
    	// ogg読み込みスレッド
    	//----------------------------------//
    	new Thread(new Runnable(){
    		@Override
    		public void run(){
    	    	try{
    	        	FileInputStream fis = new FileInputStream (outputFilePath);
    	        	byte input_buf[] = new byte[fis.available()];
    	        	fis.read(input_buf);
    	        	convertOggToPCMJNIStartofMemory(input_buf,input_buf.length);
    	    		fis.close();
    	    	}catch(Exception e){
    	    		return;
    	    	}
    			buf_len=android.media.AudioTrack.getMinBufferSize(convertOggToPCMJNIGetFS(),convertOggToPCMJNIGetCH(),AudioFormat.ENCODING_PCM_16BIT);
    	    	for(int i = 0 ; i < 30 ; i++ ){
    	    		audioBuffer[i] = new byte[buf_len];
    	    	}

    			// oggファイルを読み込み＆ogg → PCM変換＆バッファにためこむ
    	    	while(true){
    	    		//バッファが空いていたら読み込み
    	    		if(rd_buf_cnt+30 > wr_buf_cnt){
	    	    		pcm_len[(wr_buf_cnt)%30] = convertOggToPCMJNIUpdate(audioBuffer[(wr_buf_cnt)%30] ,buf_len);
						if( pcm_len[wr_buf_cnt%30] == 0 ){
							play_flg = false;
							break;
						}
						wr_buf_cnt++;
    	    		}
	    			try{
	    				Thread.sleep(10);
	       	    	}catch(Exception e){
	    	    	}
    	    	}
    	    	convertOggToPCMJNIEnd();
    		}
    	}).start();

    	//----------------------------------//
    	// 音声再生スレッド
    	//----------------------------------//
    	new Thread(new Runnable(){
    		@Override
    		public void run(){

    			//初回の読み込みを待つ
	    		while(wr_buf_cnt == 0){
	    			try{
	    				Thread.sleep(10);
	       	    	}catch(Exception e){
	    	    	}
   	    		}
    	    	try{
    	    		//音声情報取得
    	   	      	int fs =  convertOggToPCMJNIGetFS();
    	   	      	int ch = 0;
    	   	      	switch(convertOggToPCMJNIGetCH()){
    	   	      	case 1:
    	   	      		ch = AudioFormat.CHANNEL_CONFIGURATION_MONO;
    	   	      		break;
    	   	      	case 2:
    	   	      		ch = AudioFormat.CHANNEL_CONFIGURATION_STEREO;
    	   	      		break;

    	   	      	};
    	   	      	//再生準備
    	   	      	int audioTrack_buf=android.media.AudioTrack.getMinBufferSize(fs,ch,AudioFormat.ENCODING_PCM_16BIT);
    	   	      	AudioTrack track = new AudioTrack(
    	   	      			AudioManager.STREAM_MUSIC,
    	   	      			fs,
    	   	      			ch,
    	   	      			AudioFormat.ENCODING_PCM_16BIT,
    	   	      			audioTrack_buf,
    	   	      			AudioTrack.MODE_STREAM);
    	   	      	//再生開始
    	   	      	track.play();

    	   	      	//読み込んだPCMを再生バッファへ
   	    			while(true){
   	    				// 読み込み済みであれば再生
   	    				if((wr_buf_cnt-1) > rd_buf_cnt){
   	    					track.write(audioBuffer[(rd_buf_cnt)%30],0,pcm_len[(rd_buf_cnt)%30]);
   	    	       	    	rd_buf_cnt++;
   	    				}
   	    				// 最後の再生
   	    				else if((wr_buf_cnt-1) == rd_buf_cnt && play_flg ==false){
   	    					track.write(audioBuffer[(rd_buf_cnt)%30],0,pcm_len[(rd_buf_cnt)%30]);
   	    					break;
   	    				}
    	    			try{
    	    				Thread.sleep(10);
    	       	    	}catch(Exception e){
    	    	    	}
    	    		}
    	    		track.stop();
    	    		track.release();

    	    		// UI変更
    			    handler.post(new Runnable() {
    			    	@Override
    			    	public void run() {
    		            	button_tx.setText("REC");
    		            	button.setEnabled(true);
    		            	debug.setText("");
    		            	//oggファイル削除
    		            	File file = new File(outputFilePath);
    		            	file.delete();
    			    	}
    			    });

    	    	}catch(Exception e){
    	    	}
    		}
    	}).start();


    }


    private native int convertPCMToOggJNIStart(String foutpath, int sampling, float quality);
    private native int convertPCMToOggJNIUpdate(byte[] buf,int size,int final_flg);
    private native int convertPCMToOggJNIEnd();
	private native int convertOggToPCMJNIStart(String finpath);
	private native int convertOggToPCMJNIStartofMemory(byte[] buf,int size);
	private native int convertOggToPCMJNIUpdate(byte[] buf,int size);
	private native int convertOggToPCMJNIEnd();
	private native int convertOggToPCMJNIGetCH();
	private native int convertOggToPCMJNIGetFS();
	private native double convertOggToPCMJNIGetTotalTime();
	private native int convertOggToPCMJNIGetTotalPCMSize();
	private native int convertOggToPCMJNISetSeekTime(double sec);
	private native int convertOggToPCMJNISetSeekPCM(int pos);
	private native double convertOggToPCMJNIGetPlayingTime();
	private native int convertMONOtoSteJNI(byte[] input,byte[] output,int size);


	static {
		System.loadLibrary("vorbis-jni");
	}

}