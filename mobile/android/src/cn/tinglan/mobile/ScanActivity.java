package cn.tinglan.mobile;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.*;
import android.hardware.Camera;
import android.os.Bundle;
import android.view.*;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Toast;
import com.google.zxing.BinaryBitmap;
import com.google.zxing.DecodeHintType;
import com.google.zxing.PlanarYUVLuminanceSource;
import com.google.zxing.common.HybridBinarizer;
import com.google.zxing.qrcode.QRCodeReader;
import java.util.EnumMap;
import java.util.Map;

/** "扫一扫": reads a desktop sign-in QR code with the camera. Frames are decoded on the device by the
 *  bundled ZXing core; no image leaves the phone and the result goes back only to MainActivity. */
@SuppressWarnings("deprecation") // android.hardware.Camera needs no support library and covers minSdk 26.
public final class ScanActivity extends Activity implements SurfaceHolder.Callback,Camera.PreviewCallback {
    static final String RESULT="text";
    private static final int PERMISSION=7;
    private Camera camera;
    private SurfaceHolder holder;
    private volatile boolean decoding=false,done=false;
    private Thread worker;
    private final QRCodeReader reader=new QRCodeReader();
    private final Map<DecodeHintType,Object> hints=new EnumMap<>(DecodeHintType.class);
    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);hints.put(DecodeHintType.CHARACTER_SET,"UTF-8");hints.put(DecodeHintType.TRY_HARDER,Boolean.TRUE);
        getWindow().setStatusBarColor(Color.BLACK);getWindow().setNavigationBarColor(Color.BLACK);
        float d=getResources().getDisplayMetrics().density;FrameLayout root=new FrameLayout(this);root.setBackgroundColor(Color.BLACK);
        SurfaceView preview=new SurfaceView(this);holder=preview.getHolder();holder.addCallback(this);root.addView(preview,new FrameLayout.LayoutParams(-1,-1));
        root.addView(new Viewfinder(this),new FrameLayout.LayoutParams(-1,-1));
        TextView hint=new TextView(this);hint.setText("将电脑上的登录二维码放入框内");hint.setTextColor(Color.WHITE);hint.setTextSize(15);hint.setGravity(Gravity.CENTER);
        FrameLayout.LayoutParams hintAt=new FrameLayout.LayoutParams(-1,-2,Gravity.BOTTOM);hintAt.bottomMargin=(int)(120*d);root.addView(hint,hintAt);
        TextView cancel=new TextView(this);cancel.setText("取消");cancel.setTextColor(Color.WHITE);cancel.setTextSize(16);cancel.setPadding((int)(24*d),(int)(14*d),(int)(24*d),(int)(14*d));cancel.setContentDescription("取消扫码");
        cancel.setOnClickListener(v->{setResult(RESULT_CANCELED);finish();});FrameLayout.LayoutParams cancelAt=new FrameLayout.LayoutParams(-2,-2,Gravity.BOTTOM|Gravity.CENTER_HORIZONTAL);cancelAt.bottomMargin=(int)(40*d);root.addView(cancel,cancelAt);
        setContentView(root);
        if(checkSelfPermission(Manifest.permission.CAMERA)!=PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{Manifest.permission.CAMERA},PERMISSION);
    }
    @Override public void onRequestPermissionsResult(int code,String[] permissions,int[] results){
        if(code!=PERMISSION)return;
        if(results.length>0&&results[0]==PackageManager.PERMISSION_GRANTED){if(holder.getSurface()!=null&&holder.getSurface().isValid())open();}
        else{Toast.makeText(this,"需要相机权限才能扫码，可在系统设置中开启",Toast.LENGTH_LONG).show();setResult(RESULT_CANCELED);finish();}
    }
    @Override protected void onResume(){super.onResume();if(holder.getSurface()!=null&&holder.getSurface().isValid())open();}
    @Override protected void onPause(){release();super.onPause();}
    public void surfaceCreated(SurfaceHolder h){open();}
    public void surfaceChanged(SurfaceHolder h,int format,int w,int height){}
    public void surfaceDestroyed(SurfaceHolder h){release();}
    private void open(){
        if(camera!=null||done||checkSelfPermission(Manifest.permission.CAMERA)!=PackageManager.PERMISSION_GRANTED)return;
        int id=-1;Camera.CameraInfo info=new Camera.CameraInfo();for(int i=0;i<Camera.getNumberOfCameras();i++){Camera.getCameraInfo(i,info);if(info.facing==Camera.CameraInfo.CAMERA_FACING_BACK){id=i;break;}}
        if(id<0&&Camera.getNumberOfCameras()>0){id=0;Camera.getCameraInfo(0,info);}
        try{
            if(id<0)throw new IllegalStateException("没有可用的相机");
            camera=Camera.open(id);Camera.Parameters params=camera.getParameters();
            Camera.Size best=null;for(Camera.Size s:params.getSupportedPreviewSizes())if(s.width<=1920&&(best==null||Math.abs(s.width*s.height-1280*720)<Math.abs(best.width*best.height-1280*720)))best=s;
            if(best!=null)params.setPreviewSize(best.width,best.height);
            if(params.getSupportedFocusModes().contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE))params.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
            camera.setParameters(params);
            int rotation;switch(getWindowManager().getDefaultDisplay().getRotation()){case Surface.ROTATION_90:rotation=90;break;case Surface.ROTATION_180:rotation=180;break;case Surface.ROTATION_270:rotation=270;break;default:rotation=0;}
            camera.setDisplayOrientation(info.facing==Camera.CameraInfo.CAMERA_FACING_FRONT?(360-(info.orientation+rotation)%360)%360:(info.orientation-rotation+360)%360);
            camera.setPreviewDisplay(holder);camera.setPreviewCallback(this);camera.startPreview();
        }catch(Exception e){release();Toast.makeText(this,"无法打开相机："+e.getMessage(),Toast.LENGTH_LONG).show();setResult(RESULT_CANCELED);finish();}
    }
    private void release(){Camera old=camera;camera=null;if(old!=null){old.setPreviewCallback(null);old.stopPreview();old.release();}}
    /** Decodes the central square of a preview frame; a QR code is found in any orientation. */
    @Override public void onPreviewFrame(byte[] data,Camera cam){
        if(decoding||done||camera==null)return;Camera.Size size;try{size=cam.getParameters().getPreviewSize();}catch(RuntimeException e){return;}
        final int w=size.width,h=size.height,side=Math.min(w,h)*3/4,left=(w-side)/2,top=(h-side)/2;final byte[] frame=data.clone();decoding=true;
        worker=new Thread(()->{
            String text=null;
            try{text=reader.decode(new BinaryBitmap(new HybridBinarizer(new PlanarYUVLuminanceSource(frame,w,h,left,top,side,side,false))),hints).getText();}
            catch(Exception ignored){}finally{reader.reset();}
            final String found=text;
            runOnUiThread(()->{decoding=false;if(found==null||done)return;done=true;release();setResult(RESULT_OK,new Intent().putExtra(RESULT,found));finish();});
        },"tinglan-scan");worker.start();
    }
    /** Darkens everything outside the scanning square and draws its corners. */
    private static final class Viewfinder extends View {
        private final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);
        Viewfinder(android.content.Context c){super(c);setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);}
        @Override protected void onDraw(Canvas c){
            float w=getWidth(),h=getHeight(),side=Math.min(w,h)*.68f,l=(w-side)/2,t=(h-side)/2-h*.06f,d=getResources().getDisplayMetrics().density;
            p.setStyle(Paint.Style.FILL);p.setColor(0x99000000);c.drawRect(0,0,w,t,p);c.drawRect(0,t+side,w,h,p);c.drawRect(0,t,l,t+side,p);c.drawRect(l+side,t,w,t+side,p);
            p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(4*d);p.setColor(0xff1689ff);p.setStrokeCap(Paint.Cap.ROUND);float k=26*d;
            c.drawLine(l,t,l+k,t,p);c.drawLine(l,t,l,t+k,p);c.drawLine(l+side,t,l+side-k,t,p);c.drawLine(l+side,t,l+side,t+k,p);
            c.drawLine(l,t+side,l+k,t+side,p);c.drawLine(l,t+side,l,t+side-k,p);c.drawLine(l+side,t+side,l+side-k,t+side,p);c.drawLine(l+side,t+side,l+side,t+side-k,p);
        }
    }
}
