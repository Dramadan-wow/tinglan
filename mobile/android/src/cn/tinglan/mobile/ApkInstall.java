package cn.tinglan.mobile;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;
import android.widget.Toast;
import org.json.JSONObject;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;

/**
 * Hands a verified APK to the system installer. Android always asks the user to confirm;
 * the APK must be this app, the announced version, newer than the installed one and signed
 * with the same certificate, so a tampered feed cannot replace the app.
 */
public final class ApkInstall {
    private ApkInstall(){}

    /** Returns null when the APK may be installed, otherwise the reason it may not. */
    @SuppressWarnings("deprecation")
    public static String verify(Context context,File apk,JSONObject release){
        try{
            PackageManager pm=context.getPackageManager();
            int flags=Build.VERSION.SDK_INT>=28?PackageManager.GET_SIGNING_CERTIFICATES:PackageManager.GET_SIGNATURES;
            PackageInfo update=pm.getPackageArchiveInfo(apk.getAbsolutePath(),flags);
            if(update==null)return "安装包无法解析";
            PackageInfo installed=pm.getPackageInfo(context.getPackageName(),flags);
            if(!context.getPackageName().equals(update.packageName))return "安装包不是听澜";
            if(!release.optString("version").equals(update.versionName))return "安装包版本与服务器公布的不一致";
            if(code(update)<=code(installed))return "安装包版本不比当前版本新";
            if(!Arrays.equals(signers(update),signers(installed)))return "安装包签名与已安装的听澜不一致";
            return null;
        }catch(Exception e){return "安装包校验失败";}
    }
    @SuppressWarnings("deprecation")
    private static long code(PackageInfo info){return Build.VERSION.SDK_INT>=28?info.getLongVersionCode():info.versionCode;}
    @SuppressWarnings("deprecation")
    private static Signature[] signers(PackageInfo info){
        Signature[] all=Build.VERSION.SDK_INT>=28?(info.signingInfo==null?null:info.signingInfo.getApkContentsSigners()):info.signatures;
        if(all==null||all.length==0)throw new IllegalStateException("unsigned");
        Signature[] sorted=all.clone();Arrays.sort(sorted,(a,b)->a.toCharsString().compareTo(b.toCharsString()));return sorted;
    }

    public static void install(Context context,File apk)throws Exception{
        PackageInstaller installer=context.getPackageManager().getPackageInstaller();
        PackageInstaller.SessionParams params=new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        params.setAppPackageName(context.getPackageName());params.setSize(apk.length());
        int id=installer.createSession(params);
        try(PackageInstaller.Session session=installer.openSession(id)){
            try(InputStream in=new FileInputStream(apk);OutputStream out=session.openWrite("tinglan.apk",0,apk.length())){
                byte[] buffer=new byte[65536];int n;while((n=in.read(buffer))>0)out.write(buffer,0,n);session.fsync(out);
            }
            Intent status=new Intent(context,Status.class).setAction("cn.tinglan.mobile.INSTALL_STATUS");
            int flags=PendingIntent.FLAG_UPDATE_CURRENT|(Build.VERSION.SDK_INT>=31?PendingIntent.FLAG_MUTABLE:0);
            session.commit(PendingIntent.getBroadcast(context,id,status,flags).getIntentSender());
        }catch(Exception e){installer.abandonSession(id);throw e;}
    }

    /** Receives the installer's progress; opens the system confirmation screen when it asks for one. */
    public static final class Status extends BroadcastReceiver {
        @SuppressWarnings("deprecation")
        @Override public void onReceive(Context context,Intent intent){
            int status=intent.getIntExtra(PackageInstaller.EXTRA_STATUS,PackageInstaller.STATUS_FAILURE);
            if(status==PackageInstaller.STATUS_PENDING_USER_ACTION){
                Intent confirm=intent.getParcelableExtra(Intent.EXTRA_INTENT);
                if(confirm!=null){confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);context.startActivity(confirm);}
            }else if(status==PackageInstaller.STATUS_FAILURE_ABORTED)Toast.makeText(context,"已取消安装，可在「我的」里重新检查更新",Toast.LENGTH_LONG).show();
            else if(status!=PackageInstaller.STATUS_SUCCESS){
                String detail=status==PackageInstaller.STATUS_FAILURE_CONFLICT||status==PackageInstaller.STATUS_FAILURE_INCOMPATIBLE?"签名或版本与已安装的听澜冲突":status==PackageInstaller.STATUS_FAILURE_STORAGE?"手机存储空间不足":"系统未能安装更新";
                Toast.makeText(context,"更新未安装："+detail,Toast.LENGTH_LONG).show();
            }
        }
    }
}
