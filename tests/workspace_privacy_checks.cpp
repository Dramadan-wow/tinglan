// A user's server snapshot must not expose other users' private profile data.
#include "WorkspaceCodecHsx.h"
#include "DemoDataHsx.h"
#include "AccountManagerHsx.h"
#include "GroupManagerHsx.h"
#include <QJsonArray>
#include <iostream>
using namespace im;
namespace {
int checks=0;
void expect(bool condition,const std::string& description){++checks;if(!condition)throw std::runtime_error("FAILED: "+description);}
QJsonObject user(const QJsonObject& snapshot,int id){for(auto v:snapshot["users"].toArray())if(v.toObject()["id"].toInt()==id)return v.toObject();throw std::runtime_error("user missing from snapshot");}
QJsonObject group(const QJsonObject& snapshot,int id){for(auto v:snapshot["groups"].toArray())if(v.toObject()["id"].toInt()==id)return v.toObject();throw std::runtime_error("group missing from snapshot");}
QJsonObject service(const QJsonObject& u,ServiceHsx type){for(auto v:u["services"].toArray())if(v.toObject()["type"].toInt()==int(type))return v.toObject();return {};}
}
int main(){
    try{
        // Demo: 10001 is a friend of 10002, 10003 and 10004; 10002 is not a friend of 10003 or 10004.
        DataStoreHsx data;DemoDataHsx::create(data);const auto qq=ServiceHsx::QQ,wc=ServiceHsx::WeChat;
        AccountManagerHsx accounts(data);accounts.login(10001,qq,"123456");accounts.login(10004,qq,"123456");
        GroupManagerHsx(data).mute(10004,qq,1005,10001,30);
        const auto view=WorkspaceCodecHsx::encode(data,10002);
        const auto self=user(view,10002),friendView=user(view,10001),stranger=user(view,10004);
        expect(self["birth"]=="1999-11-11"&&self["location"]=="北京"&&self["applied"]=="2017-09-01","viewer keeps own full profile");
        expect(service(self,wc)["bound"]=="10002","viewer keeps own WeChat binding");
        expect(friendView["location"]=="长春"&&friendView["online"].toBool(),"friend location and presence stay visible");
        expect(friendView["birth"]!="2000-05-01"&&friendView["applied"]!="2016-09-01","friend birth and registration dates are hidden");
        expect(service(friendView,wc)["bound"]=="0","friend WeChat-QQ binding is hidden");
        expect(stranger["location"]!="广州"&&!stranger["online"].toBool()&&stranger["birth"]!="2000-12-12","non-friend location, presence and birth are hidden");
        expect(stranger["name"]=="赵六"&&service(stranger,qq)["number"]=="10004","non-friend keeps public name and service number for search");
        expect(service(stranger,qq)["friends"].toArray().isEmpty()&&service(stranger,qq)["password"].toString().isEmpty(),"other users' friends and credentials are never sent");
        expect(group(view,1005)["mutes"].toArray().isEmpty(),"mute list hidden from non-members");
        expect(!group(WorkspaceCodecHsx::encode(data,10001),1005)["mutes"].toArray().isEmpty(),"group members still see mutes");
        // Placeholders must stay decodable by already-installed clients.
        const auto decoded=WorkspaceCodecHsx::decode(view);
        expect(decoded.users().size()==4&&decoded.user(10004).name()=="赵六","trimmed snapshot decodes");
        const auto admin=WorkspaceCodecHsx::encode(data,-1),full=WorkspaceCodecHsx::encode(data);
        expect(user(admin,10004)["location"]=="广州"&&user(admin,10004)["online"].toBool()&&user(admin,10003)["birth"]=="2001-03-03","administrator view keeps profiles");
        expect(service(user(admin,10001),qq)["password"].toString().isEmpty(),"administrator view masks credentials");
        expect(user(full,10004)["birth"]=="2000-12-12"&&!service(user(full,10004),qq)["password"].toString().isEmpty(),"server backup encoding is unchanged");
        std::cout<<"PASS "<<checks<<" workspace privacy checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
