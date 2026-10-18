package cn.tinglan.mobile;

import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public final class Wire {
    public static final int MAX_FRAME=65536;
    public static JSONObject obj(Object... args) {
        JSONObject out=new JSONObject();
        try {for(int i=0;i<args.length;i+=2) out.put((String)args[i],args[i+1]);}
        catch(Exception e){throw new IllegalArgumentException(e);} return out;
    }
    public static String id(){return UUID.randomUUID().toString();}
    static String utf8(byte[] bytes) throws Exception {
        return StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT).onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes)).toString();
    }
    public static JSONObject envelope(String type,JSONObject data,String sender,String id) {
        return obj("v",1,"type",type,"id",id,"senderId",sender,"recipientId","","data",data);
    }
    public static byte[] encode(JSONObject message) throws Exception {
        byte[] body=message.toString().getBytes(StandardCharsets.UTF_8); ConnectionProfile.require(body.length>0&&body.length<=MAX_FRAME,"消息过长");
        return ByteBuffer.allocate(body.length+4).putInt(body.length).put(body).array();
    }
    public static final class Decoder {
        private final ByteArrayOutputStream buffer=new ByteArrayOutputStream();
        public List<JSONObject> feed(byte[] bytes,int length) throws Exception {
            ConnectionProfile.require(buffer.size()+length<=2*MAX_FRAME+4,"接收缓冲区超限");buffer.write(bytes,0,length);
            byte[] all=buffer.toByteArray(); int offset=0; List<JSONObject> messages=new ArrayList<>();
            while(all.length-offset>=4){
                int size=ByteBuffer.wrap(all,offset,4).getInt(); ConnectionProfile.require(size>0&&size<=MAX_FRAME,"服务器帧长度无效");
                if(all.length-offset<size+4) break;
                JSONObject m=new JSONObject(utf8(java.util.Arrays.copyOfRange(all,offset+4,offset+4+size))); validate(m);messages.add(m);offset+=4+size;
            }
            buffer.reset(); buffer.write(all,offset,all.length-offset);return messages;
        }
        private void validate(JSONObject m) throws Exception {
            ConnectionProfile.require(m.getDouble("v")==1&&m.get("id") instanceof String&&m.getString("id").matches("[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}")&&m.get("type") instanceof String&&m.getString("type").length()>0&&m.getString("type").length()<=32&&m.get("senderId") instanceof String&&m.get("recipientId") instanceof String&&m.getString("senderId").length()<=32&&m.getString("recipientId").length()<=32&&m.get("data") instanceof JSONObject,"服务器协议无效");
        }
    }
}
