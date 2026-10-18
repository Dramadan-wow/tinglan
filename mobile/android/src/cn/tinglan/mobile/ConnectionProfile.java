package cn.tinglan.mobile;

import org.json.JSONObject;
import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.Base64;
import java.util.HashSet;
import java.util.Iterator;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManagerFactory;

/** The desktop TINGLAN1 format. No trust-all or plaintext fallback is supported. */
public final class ConnectionProfile {
    public final String host, ca, mode, fingerprint;
    public final int port;
    private ConnectionProfile(String host, int port, String ca, String mode, String fingerprint) {
        this.host=host; this.port=port; this.ca=ca; this.mode=mode; this.fingerprint=fingerprint;
    }
    static void require(boolean value, String message) { if(!value) throw new IllegalArgumentException(message); }
    static String hex(byte[] bytes) {
        StringBuilder out=new StringBuilder(); for(byte b:bytes) out.append(String.format(java.util.Locale.ROOT,"%02x",b&255)); return out.toString();
    }
    public static ConnectionProfile parse(String code) throws Exception {
        code=code.trim(); require(code.length()<=16384,"连接码过长"); String[] p=code.split("\\.",-1);
        require(p.length==3&&p[0].equals("TINGLAN1"),"请粘贴 TINGLAN1. 开头的完整连接码");
        byte[] raw=Base64.getUrlDecoder().decode(p[1]);
        require(Base64.getUrlEncoder().withoutPadding().encodeToString(raw).equals(p[1]),"连接码编码无效");
        require(hex(MessageDigest.getInstance("SHA-256").digest(raw)).substring(0,16).equals(p[2]),"连接码不完整或已损坏");
        JSONObject json=new JSONObject(Wire.utf8(raw));
        HashSet<String> allowed=new HashSet<>(Arrays.asList("version","host","port","ca","mode"));
        for(Iterator<String> it=json.keys();it.hasNext();) require(allowed.contains(it.next()),"连接码含有不支持的字段");
        require(json.getInt("version")==1&&json.getDouble("version")==1,"连接码版本无效");
        String host=json.getString("host"),ca=json.getString("ca"),mode=json.getString("mode");
        int port=json.getInt("port"); require(port>0&&port<=65535&&json.getDouble("port")==port,"端口无效");
        require(mode.equals("cloud")||mode.equals("lan"),"连接模式无效");
        require(host.length()<=253&&host.matches("[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?"),"服务器地址无效");
        if(host.matches("[0-9.]+")) {
            String[] parts=host.split("\\.",-1); require(parts.length==4,"IPv4 地址无效");
            for(String part:parts) require(part.matches("0|[1-9][0-9]{0,2}")&&Integer.parseInt(part)<=255,"IPv4 地址无效");
            require(!host.equals("0.0.0.0")&&Integer.parseInt(parts[0])<224,"服务器 IP 地址无效");
        } else for(String part:host.split("\\.",-1)) require(part.length()<=63&&part.matches("[A-Za-z0-9](?:[A-Za-z0-9-]*[A-Za-z0-9])?"),"域名无效");
        require(ca.getBytes(StandardCharsets.UTF_8).length<=8192&&!ca.contains("PRIVATE KEY")&&!ca.contains("PUBLIC KEY"),"连接码不得包含密钥");
        String fingerprint="系统信任证书";
        if(!ca.isEmpty()) {
            require(ca.trim().startsWith("-----BEGIN CERTIFICATE-----")&&ca.trim().endsWith("-----END CERTIFICATE-----")&&ca.indexOf("-----BEGIN",1)<0,"需要单个公共 CA 证书");
            X509Certificate cert=certificate(ca); cert.checkValidity(); fingerprint=hex(MessageDigest.getInstance("SHA-256").digest(cert.getEncoded()));
        }
        return new ConnectionProfile(host,port,ca,mode,fingerprint);
    }
    private static X509Certificate certificate(String pem) throws Exception {
        return (X509Certificate)CertificateFactory.getInstance("X.509").generateCertificate(new ByteArrayInputStream(pem.getBytes(StandardCharsets.US_ASCII)));
    }
    public SSLContext tls() throws Exception {
        TrustManagerFactory tm=TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
        if(ca.isEmpty()) tm.init((KeyStore)null);
        else {KeyStore store=KeyStore.getInstance(KeyStore.getDefaultType()); store.load(null,null); store.setCertificateEntry("tinglan",certificate(ca)); tm.init(store);}
        SSLContext context=SSLContext.getInstance("TLS"); context.init(null,tm.getTrustManagers(),null); return context;
    }
}
