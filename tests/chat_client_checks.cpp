// The login dialog reacts to problem(): a workspace connection lost before the first login must be
// reported once and must not keep reconnecting with the password, while a signed-in session that
// drops keeps reconnecting quietly. Uses a plaintext loopback server, which the client allows only
// for an explicit 127.0.0.1 test.
#include "ChatClientHsx.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <cstdio>
#include <memory>
#include <stdexcept>
using namespace im::net;
namespace {
int checks=0;
void expect(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(std::string("FAILED: ")+what);}
void spin(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms)QCoreApplication::processEvents(QEventLoop::AllEvents,20);}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    try{
        QTcpServer server;expect(server.listen(QHostAddress::LocalHost,0),"loopback test server listens");
        bool accept=false;int connections=0;QList<QTcpSocket*> peers;
        QObject::connect(&server,&QTcpServer::newConnection,[&]{while(auto* s=server.nextPendingConnection()){++connections;
            if(!accept){s->abort();s->deleteLater();continue;}
            peers.append(s);auto decoder=std::make_shared<DecoderHsx>();
            QObject::connect(s,&QTcpSocket::readyRead,[s,decoder]{QList<QJsonObject> messages;QString error;if(!decoder->feed(s->readAll(),messages,error))return;
                for(const auto& m:messages)if(m["type"]=="login")s->write(encode(envelope("login_result",{{"requestId",m["id"].toString()},{"ok",true},{"userId","u10001"},{"workspace",true},{"admin",false}},{},"u10001")));});
        }});
        ChatClientHsx client;QStringList problems;QObject::connect(&client,&ChatClientHsx::problem,[&](const QString& text){problems<<text;});
        ClientOptionsHsx options;options.host="127.0.0.1";options.port=server.serverPort();options.username="checker";options.password="check-password-2026";options.workspace=true;options.tls=false;options.allowInsecureLoopback=true;
        client.start(options);spin(3500);
        expect(connections==1,"no background reconnect after a drop before the first login");
        expect(problems.size()==1,"a drop before the first login is reported exactly once");
        problems.clear();client.start(options);spin(1500);
        expect(connections==2&&problems.size()==1&&problems[0]!="未连接","a new attempt reports only its own failure");
        accept=true;problems.clear();client.start(options);spin(1000);
        expect(client.authenticated(),"loopback login succeeds");
        const int before=connections;for(auto* peer:peers)peer->abort();peers.clear();spin(2500);
        expect(connections>before&&client.authenticated(),"a signed-in session reconnects after a drop");
        expect(problems.isEmpty(),"reconnecting a signed-in session reports no login failure");
        client.stop();
        std::printf("PASS %d chat client checks\n",checks);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
