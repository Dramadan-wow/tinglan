package cn.tinglan.mobile.tests;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

/** Runs only in a dedicated emulator. Fixtures never enter the production APK. */
public final class UiChecks extends Instrumentation {
    private Activity activity;
    private int checks;
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    private void set(String name,Object value)throws Exception{Field field=activity.getClass().getDeclaredField(name);field.setAccessible(true);field.set(activity,value);}
    private void call(String name)throws Exception{Method method=activity.getClass().getDeclaredMethod(name);method.setAccessible(true);method.invoke(activity);}
    private void main(Runnable action){runOnMainSync(action);waitForIdleSync();}
    private boolean find(View view,String target){
        if(view instanceof TextView&&((TextView)view).getText().toString().contains(target))return true;
        if(view.getContentDescription()!=null&&view.getContentDescription().toString().contains(target))return true;
        if(view instanceof ViewGroup)for(int i=0;i<((ViewGroup)view).getChildCount();i++)if(find(((ViewGroup)view).getChildAt(i),target))return true;
        return false;
    }
    private void check(String target){final boolean[] result={false};main(()->result[0]=find(activity.getWindow().getDecorView(),target));if(!result[0])throw new AssertionError("Missing native UI: "+target);checks++;}
    private void screenshot(String name)throws Exception{
        waitForIdleSync();Thread.sleep(700);Bitmap bitmap=getUiAutomation().takeScreenshot();if(bitmap==null)throw new AssertionError("No rendered screenshot");
        File output=new File(getTargetContext().getExternalFilesDir(null),"ui-proof");if(!output.exists()&&!output.mkdirs())throw new AssertionError("Screenshot directory failed");
        try(FileOutputStream stream=new FileOutputStream(new File(output,name+".png"))){bitmap.compress(Bitmap.CompressFormat.PNG,100,stream);}bitmap.recycle();
    }
    private JSONObject fixture()throws Exception{
        JSONObject root=new JSONObject();root.put("version",1).put("viewer",1).put("admin",false);
        JSONArray users=new JSONArray();for(int id=1;id<=3;id++){JSONObject u=new JSONObject();u.put("id",id).put("name",id==1?"林间晚风":id==2?"晴空来信":"山野手记").put("location","在生活里寻找灵感").put("online",id!=3);JSONArray services=new JSONArray();for(int type=0;type<3;type++)services.put(new JSONObject().put("type",type).put("nick",u.getString("name")).put("number",String.valueOf(id)).put("friends",new JSONArray()).put("blocked",new JSONArray()));u.put("services",services);users.put(u);}root.put("users",users);
        root.put("groups",new JSONArray()).put("messages",new JSONArray()).put("requests",new JSONArray()).put("feed",new JSONArray()).put("appearances",new JSONObject()).put("follows",new JSONArray().put(2));
        JSONArray posts=new JSONArray();posts.put(new JSONObject().put("id","1").put("author",2).put("text","#今天的小确幸#\n把喜欢的瞬间收集起来，平凡的日子也会闪闪发光。\n\n这一刻，与你同频。").put("created",1792276200).put("image","").put("likes",18).put("liked",true).put("comments",new JSONArray().put(new JSONObject().put("author",3).put("text","好喜欢这样的生活节奏！"))));posts.put(new JSONObject().put("id","2").put("author",3).put("text","日落不会迟到，快乐也是。今天也拍到了喜欢的天空。").put("created",1792272000).put("image","").put("likes",6).put("liked",false).put("comments",new JSONArray()));root.put("weibo",posts);
        root.put("directMessages",new JSONArray().put(new JSONObject().put("id","1").put("sender",2).put("recipient",1).put("post","1").put("text","谢谢你分享的灵感，一起记录生活吧。").put("created",1792276200).put("read",true)));
        return root;
    }
    @Override public void onStart(){Bundle result=new Bundle();try{
        getTargetContext().getSharedPreferences("connection",0).edit().clear().commit();
        Intent intent=new Intent().setClassName(getTargetContext(),"cn.tinglan.mobile.MainActivity").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity=startActivitySync(intent);waitForIdleSync();check("欢迎回来");check("听澜山水主题");screenshot("01-login-river-phone");
        main(()->{try{set("service",2);call("showLogin");}catch(Exception e){throw new RuntimeException(e);}});check("伊冯灵感主题");check("伊冯灵感 · 记录属于你的频率");screenshot("02-login-yvonne-phone");
        JSONObject data=fixture();Class<?> workspace=getTargetContext().getClassLoader().loadClass("cn.tinglan.mobile.Workspace");Object model=workspace.getConstructor(JSONObject.class).newInstance(data);
        main(()->{try{set("workspace",model);set("service",2);set("tab",2);set("channel","weibo");set("connectionState","界面验证 · 隔离示例数据");call("showHome");}catch(Exception e){throw new RuntimeException(e);}});
        check("伊冯灵感主题");check("晴空来信");check("关注");check("私信");screenshot("03-weibo-phone");
        main(()->{try{set("weiboFilter",1);call("showHome");}catch(Exception e){throw new RuntimeException(e);}});check("我的关注");check("晴空来信");screenshot("04-following-phone");
        main(()->{try{set("weiboInbox",true);call("showHome");}catch(Exception e){throw new RuntimeException(e);}});check("微博私信");check("晴空来信");screenshot("05-inbox-phone");
        // A density change recreates activities; never assert against a detached old view tree.
        main(()->activity.finish());
        getUiAutomation().executeShellCommand("wm size 1600x1000").close();getUiAutomation().executeShellCommand("wm density 200").close();Thread.sleep(2500);waitForIdleSync();
        activity=startActivitySync(intent);waitForIdleSync();
        main(()->{try{set("workspace",model);set("service",2);set("tab",2);set("channel","weibo");set("connectionState","界面验证 · 隔离示例数据");set("weiboInbox",false);set("weiboFilter",0);call("showHome");}catch(Exception e){throw new RuntimeException(e);}});check("伊冯灵感主题");check("晴空来信");screenshot("06-weibo-tablet");
        main(()->{try{set("workspace",null);set("service",2);call("showLogin");}catch(Exception e){throw new RuntimeException(e);}});check("欢迎回来");screenshot("07-login-tablet");
        result.putString("result","PASS "+checks+" native UI checks; original artwork rendered");finish(Activity.RESULT_OK,result);
    }catch(Throwable e){result.putString("failure",e.toString());finish(Activity.RESULT_CANCELED,result);}}
}
