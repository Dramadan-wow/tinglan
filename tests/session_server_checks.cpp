// Multi-device sessions on one account: desktop and phone together, a newer sign-in replacing the older
// one of the same kind, revocable phone sign-in tokens and QR sign-in confirmed from the phone.
// Runs a real server on a plaintext loopback listener, which only an explicit 127.0.0.1 test may use.
#include "AuthStoreHsx.h"
#include "ChatClientHsx.h"
#include "ChatServerHsx.h"
#include "PasswordHsx.h"
#include "WorkspaceBackendHsx.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUuid>
#include <cstdio>
#include <memory>
#include <stdexcept>
using namespace im::net;
namespace {
int checks=0;
void expect(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(std::string("FAILED: ")+what);}
void spin(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms)QCoreApplication::processEvents(QEventLoop::AllEvents,20);}
const QString Password="session-check-2026";
// A bare protocol client, so the checks see exactly what the server sends.
struct Peer {
    QTcpSocket socket;DecoderHsx decoder;QList<QJsonObject> inbox;bool closed=false;QString user;
    explicit Peer(quint16 port){
        QObject::connect(&socket,&QTcpSocket::readyRead,[this]{QList<QJsonObject> messages;QString error;decoder.feed(socket.readAll(),messages,error);inbox+=messages;});
        QObject::connect(&socket,&QTcpSocket::disconnected,[this]{closed=true;});
        socket.connectToHost(QHostAddress::LocalHost,port);socket.waitForConnected(3000);
    }
    QString send(const QString& type,const QJsonObject& data){const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);socket.write(encode(envelope(type,data,user,{},id)));return id;}
    QJsonObject wait(const QString& type,int ms=15000){
        QElapsedTimer t;t.start();
        while(t.elapsed()<ms){for(int i=0;i<inbox.size();++i)if(inbox[i]["type"]==type)return inbox.takeAt(i)["data"].toObject();QCoreApplication::processEvents(QEventLoop::AllEvents,20);}
        return {};
    }
    QJsonObject login(QJsonObject data){data["workspace"]=true;data["service"]=0;const auto id=send("login",data);auto r=wait("login_result");if(r["ok"].toBool())user=r["userId"].toString();expect(r["requestId"]==id,"login result answers its request");return r;}
    bool waitClosed(int ms=3000){QElapsedTimer t;t.start();while(!closed&&t.elapsed()<ms)QCoreApplication::processEvents(QEventLoop::AllEvents,20);return closed;}
    QString error(int ms=3000){return wait("error",ms)["code"].toString();}
};
QJsonObject withPassword(const QString& device,bool remember=false){return {{"username","会话测试"},{"password",Password},{"device",device},{"remember",remember},{"fileTransfer",device=="desktop"?1:0}};}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        QTemporaryDir dir;expect(dir.isValid(),"temporary server directory");
        im::WorkspaceBackendHsx backend(dir.filePath("workspace.sqlite"));AuthStoreHsx auth(dir.filePath("users.json"));
        const int user=backend.registerUser("会话测试",im::PasswordHsx::hash(Password.toStdString()),im::ServiceHsx::QQ);
        ServerOptionsHsx options;options.address=QHostAddress::LocalHost;options.port=0;options.tls=false;options.allowInsecureLoopback=true;
        ChatServerHsx server(auth,options);server.setWorkspace(&backend);QString failure;expect(server.start(failure),"loopback server starts");const auto port=server.serverPort();
        const auto stable="u"+QString::number(user);static const QRegularExpression token("^[0-9a-f]{64}$");

        Peer desktop(port);expect(desktop.login(withPassword("desktop"))["ok"].toBool(),"desktop signs in with the password");
        Peer phone(port);const auto signedIn=phone.login(withPassword("mobile",true));
        expect(signedIn["ok"].toBool()&&signedIn["device"]=="mobile"&&token.match(signedIn["token"].toString()).hasMatch(),"phone signs in and receives an automatic sign-in token");
        expect(!desktop.waitClosed(800),"desktop and phone stay signed in together");
        Peer second(port);const auto replacement=second.login(withPassword("desktop",true));
        expect(replacement["ok"].toBool()&&!replacement.contains("token"),"a second desktop signs in; only phones get automatic sign-in tokens");
        expect(desktop.error()=="session_replaced"&&desktop.waitClosed(),"a newer desktop sign-in replaces the older desktop");
        expect(!phone.waitClosed(500),"replacing the desktop leaves the phone signed in");
        Peer legacy(port);expect(legacy.login({{"username","会话测试"},{"password",Password},{"fileTransfer",1}})["ok"].toBool(),"a client from before 5.7 still signs in");
        expect(second.error()=="session_replaced","the old desktop client counts as a desktop");
        Peer third(port);expect(third.login(withPassword("desktop"))["ok"].toBool(),"desktop signs in again");
        expect(legacy.error()=="session_revoked"&&legacy.waitClosed(),"an old client is told session_revoked, which it treats as final");

        const auto saved=signedIn["token"].toString();
        Peer resumed(port);const auto viaToken=resumed.login({{"token",saved},{"device","mobile"}});
        expect(viaToken["ok"].toBool()&&viaToken["numericId"].toInt()==user&&viaToken["device"]=="mobile","the saved token signs the phone in without a password");
        expect(phone.error()=="session_replaced","the token sign-in replaces the previous phone connection");
        Peer forged(port);expect(forged.login({{"token",QString(64,'a')},{"device","mobile"}})["code"]=="invalid_token","an unknown token is refused");
        // Signing in elsewhere keeps the account usable: commands from the phone still work after the desktop leaves.
        third.socket.abort();spin(300);
        const auto sync=resumed.send("workspace_command",{{"op","sync"},{"service",0}});expect(resumed.wait("workspace_result")["requestId"]==sync,"the phone keeps working after the desktop disconnects");
        resumed.send("logout",{{"token",saved}});expect(resumed.wait("logout_result")["ok"].toBool()&&resumed.waitClosed(),"signing out on the phone closes its session");
        Peer after(port);expect(after.login({{"token",saved},{"device","mobile"}})["code"]=="invalid_token","a signed-out token no longer works");
        Peer again(port);const auto fresh=again.login(withPassword("mobile",true))["token"].toString();
        backend.command(0,im::ServiceHsx::Count,QUuid::createUuid().toString(),{{"op","admin.reset"},{"target",user},{"scope",0},{"password","reset-password-2026"}},true);
        Peer reset(port);expect(reset.login({{"token",fresh},{"device","mobile"}})["code"]=="invalid_token","an administrator password reset revokes saved tokens");
        backend.command(0,im::ServiceHsx::Count,QUuid::createUuid().toString(),{{"op","admin.reset"},{"target",user},{"scope",0},{"password",Password}},true);

        Peer mobile(port);expect(mobile.login(withPassword("mobile"))["ok"].toBool(),"phone signs in for QR checks");
        Peer screen(port);const auto start=screen.send("qr_start",{{"service",0},{"device","desktop"},{"fileTransfer",1}});
        const auto ticket=screen.wait("qr_ticket");const auto code=ticket["code"].toString();
        expect(ticket["requestId"]==start&&code.size()==32&&ticket["expires"].toInt()==120,"the desktop receives a two-minute QR ticket");
        mobile.send("qr_confirm",{{"code",code},{"approve",true}});expect(!mobile.wait("qr_result")["ok"].toBool(),"confirming before scanning is refused");
        mobile.send("qr_scan",{{"code",code}});const auto scanned=mobile.wait("qr_result");
        expect(scanned["ok"].toBool()&&scanned["stage"]=="scan"&&scanned["service"].toInt()==0&&scanned["address"]=="127.0.0.1","the phone scans the code and sees where the computer connects from");
        expect(screen.wait("qr_status")["state"]=="scanned","the desktop shows that the code was scanned");
        mobile.send("qr_confirm",{{"code",code},{"approve",true}});const auto approved=screen.wait("login_result");
        expect(approved["ok"].toBool()&&approved["requestId"]==start&&approved["numericId"].toInt()==user&&approved["device"]=="desktop"&&token.match(approved["token"].toString()).hasMatch(),"confirming on the phone signs the desktop in");
        expect(mobile.wait("qr_result")["stage"]=="confirm"&&!mobile.waitClosed(500),"the phone stays signed in after approving");
        mobile.send("qr_scan",{{"code",code}});expect(!mobile.wait("qr_result")["ok"].toBool(),"a used code cannot be scanned again");
        Peer refused(port);refused.send("qr_start",{{"service",0},{"device","desktop"}});const auto other=refused.wait("qr_ticket")["code"].toString();
        mobile.send("qr_scan",{{"code",other}});mobile.wait("qr_result");expect(refused.wait("qr_status")["state"]=="scanned","the second code is scanned");mobile.send("qr_confirm",{{"code",other},{"approve",false}});
        expect(refused.wait("qr_status")["state"]=="cancelled"&&refused.waitClosed(),"declining on the phone cancels the desktop sign-in");
        mobile.wait("qr_result");

        // The desktop client end to end: request a code, the phone approves, the client signs in.
        ChatClientHsx client;QString shown;QObject::connect(&client,&ChatClientHsx::qrTicket,[&](const QString& text,int){shown=text;});
        ClientOptionsHsx qr;qr.host="127.0.0.1";qr.port=port;qr.workspace=true;qr.tls=false;qr.allowInsecureLoopback=true;qr.qrLogin=true;client.start(qr);
        QElapsedTimer waited;waited.start();while(shown.isEmpty()&&waited.elapsed()<5000)QCoreApplication::processEvents(QEventLoop::AllEvents,20);
        expect(shown.startsWith("TINGLANQR1:")&&shown.size()==43,"the desktop client shows the code text to scan");
        mobile.send("qr_scan",{{"code",shown.mid(11)}});mobile.wait("qr_result");mobile.send("qr_confirm",{{"code",shown.mid(11)},{"approve",true}});mobile.wait("qr_result");
        waited.restart();while(!client.authenticated()&&waited.elapsed()<5000)QCoreApplication::processEvents(QEventLoop::AllEvents,20);
        expect(client.authenticated()&&client.user()==stable,"the desktop client is signed in as the phone's account");
        client.stop();server.shutdown();
        std::printf("PASS %d session server checks\n",checks);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
