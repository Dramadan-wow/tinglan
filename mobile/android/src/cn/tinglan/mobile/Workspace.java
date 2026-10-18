package cn.tinglan.mobile;

import org.json.JSONArray;
import org.json.JSONObject;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** A server snapshot, scoped to the authenticated viewer. No local demo accounts. */
public final class Workspace {
    public final JSONObject raw;
    public final int viewer;
    public final Map<Integer,JSONObject> users=new LinkedHashMap<>(),groups=new LinkedHashMap<>();
    public Workspace(JSONObject raw) throws Exception {
        this.raw=raw;viewer=raw.getInt("viewer");ConnectionProfile.require(viewer>0&&!raw.optBoolean("admin"),"普通用户同步身份无效");
        JSONArray list=raw.getJSONArray("users");for(int i=0;i<list.length();i++){JSONObject u=list.getJSONObject(i);users.put(u.getInt("id"),u);}
        list=raw.getJSONArray("groups");for(int i=0;i<list.length();i++){JSONObject g=list.getJSONObject(i);groups.put(g.getInt("id"),g);}
        raw.getJSONArray("messages");raw.getJSONArray("requests");ConnectionProfile.require(users.containsKey(viewer),"同步缺少本人资料");
    }
    public JSONArray array(String key){JSONArray a=raw.optJSONArray(key);return a==null?new JSONArray():a;}
    public JSONObject account(int user,int service){JSONObject u=users.get(user);if(u!=null){JSONArray a=u.optJSONArray("services");if(a!=null)for(int i=0;i<a.length();i++){JSONObject s=a.optJSONObject(i);if(s!=null&&s.optInt("type",-1)==service)return s;}}return new JSONObject();}
    public String name(int user){JSONObject u=users.get(user);return u==null?"用户 "+user:u.optString("name","用户 "+user);}
    public String display(int user,int service){JSONArray f=account(viewer,service).optJSONArray("friends");if(f!=null)for(int i=0;i<f.length();i++){JSONObject r=f.optJSONObject(i);if(r!=null&&r.optInt("id")==user&&!r.optString("remark").isEmpty())return r.optString("remark");}return name(user);}
    public boolean member(JSONObject group,int service){JSONArray members=group.optJSONArray("members");if(members!=null)for(int i=0;i<members.length();i++){JSONObject m=members.optJSONObject(i);if(m!=null&&m.optInt("id")==viewer&&contains(m.optJSONArray("services"),service))return true;}return false;}
    public static boolean contains(JSONArray a,int id){if(a!=null)for(int i=0;i<a.length();i++)if(a.optInt(i,-1)==id)return true;return false;}
    public List<JSONObject> history(int service,int kind,int target){
        List<JSONObject> out=new ArrayList<>();JSONArray all=array("messages");
        for(int i=0;i<all.length();i++){JSONObject m=all.optJSONObject(i);if(m==null||m.optInt("kind",-1)!=kind)continue;
            if(kind==1){JSONObject group=groups.get(target);if(m.optInt("target")==target&&group!=null&&member(group,service))out.add(m);}
            else if((m.optInt("sender")==viewer&&m.optInt("source",-1)==service&&m.optInt("target")==target)||(m.optInt("target")==viewer&&m.optInt("destination",-1)==service&&m.optInt("sender")==target))out.add(m);
        }
        Collections.sort(out,(a,b)->Long.compare(a.optLong("id"),b.optLong("id")));return out;
    }
    public List<Conversation> conversations(int service){
        List<Conversation> out=new ArrayList<>();JSONArray friends=account(viewer,service).optJSONArray("friends");
        if(friends!=null)for(int i=0;i<friends.length();i++){JSONObject f=friends.optJSONObject(i);if(f!=null)out.add(conversation(service,0,f.optInt("id"),display(f.optInt("id"),service)));}
        for(JSONObject g:groups.values())if(member(g,service))out.add(conversation(service,1,g.optInt("id"),g.optString("name")));
        Collections.sort(out,(a,b)->Long.compare(b.time,a.time));return out;
    }
    private Conversation conversation(int service,int kind,int target,String title){
        List<JSONObject> messages=history(service,kind,target);Conversation c=new Conversation();c.kind=kind;c.target=target;c.title=title;c.preview=kind==1?"群聊已加入":"开始聊聊吧";
        for(JSONObject m:messages){c.time=m.optLong("created");c.preview=m.optBoolean("recalled")?"[消息已撤回]":m.optString("text");if(m.optInt("sender")!=viewer&&!m.optBoolean("recalled")&&!contains(m.optJSONArray("read"),viewer))c.unread++;}return c;
    }
    public static final class Conversation{public int kind,target,unread;public long time;public String title,preview;}
}
