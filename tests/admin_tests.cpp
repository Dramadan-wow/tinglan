#include "AccountManagerHsx.h"
#include "AdminManagerHsx.h"
#include "DemoDataHsx.h"
#include "MessageManagerHsx.h"
#include "PasswordHsx.h"
#include "StorageHsx.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#define NOMINMAX
#include <windows.h>
using namespace im;
namespace {
int checks=0;
void expect(bool ok,const char* name){++checks;if(!ok)throw std::runtime_error(name);}
void rejects(const std::function<void()>& action,const char* name){bool refused=false;try{action();}catch(const std::exception&){refused=true;}expect(refused,name);}
std::string contents(const std::filesystem::path& path){std::ifstream in(path);return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
}
int main(){
    try{
        const auto output=std::filesystem::temp_directory_path()/std::filesystem::u8path("微联-admin-"+std::to_string(now())+"-"+std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(output);
        DataStoreHsx data;DemoDataHsx::create(data);AccountManagerHsx accounts(data);AdminManagerHsx admin(data);MessageManagerHsx messages(data);
        const auto qq=ServiceHsx::QQ,wc=ServiceHsx::WeChat;
        rejects([&]{admin.forceLogout(10001);},"anonymous administrator operation denied");
        accounts.login(10001,qq,"123456");
        rejects([&]{admin.setLoginAllowed(10002,false);},"ordinary user and group owner cannot disable users");
        expect(data.user(10002).loginAllowed(),"unauthorized failure leaves account enabled");
        admin.initialize();const auto initialized=data.adminLogs().size();admin.initialize();expect(data.adminLogs().size()==initialized,"initialization never resets credential");
        rejects([&]{admin.login("admin","wrong");},"wrong administrator password denied");
        rejects([&]{admin.login("10001","123456");},"ordinary credential cannot enter administrator mode");
        expect(!admin.active(),"failed authentication does not create privilege");
        admin.login("admin","admin123456");expect(admin.active(),"independent administrator authentication");
        accounts.login(10002,qq,"123456");admin.forceLogout(10002);expect(!data.user(10002).online(),"force logout invalidates session");
        rejects([&]{messages.sendPrivate(10002,qq,10001,"过期会话");},"logged out user cannot continue sending");
        accounts.login(10002,qq,"123456");admin.setLoginAllowed(10002,false);expect(!data.user(10002).online()&&!data.user(10002).loginAllowed(),"disabling immediately logs user out");
        rejects([&]{accounts.login(10002,qq,"123456");},"disabled account cannot login with correct password");
        expect(data.adminLogs().rbegin()->second.action().find("登录失败")!=std::string::npos,"blocked login attempt recorded without password");
        rejects([&]{accounts.profile(10002,"改名","北京");},"disabled account cannot mutate profile");
        admin.setLoginAllowed(10002,true);expect(!data.user(10002).online(),"enable does not automatically login");accounts.login(10002,qq,"123456");
        admin.resetPassword(10002,qq,"QQ-new-2026");expect(!data.user(10002).online(),"reset password revokes session");
        rejects([&]{accounts.login(10002,qq,"123456");},"old selected-service password rejected");accounts.login(10002,qq,"QQ-new-2026");expect(data.user(10002).online(),"reset password authenticates");
        accounts.login(10002,wc,"123456");expect(data.user(10002).online(),"scoped reset preserves other service password");
        admin.resetPassword(10002,ServiceHsx::Count,"All-new-2026");accounts.login(10002,qq,"All-new-2026");accounts.login(10002,wc,"All-new-2026");
        expect(data.user(10002).online(),"all-service reset updates every service");
        rejects([&]{accounts.login(10002,wc,"123456");},"old other-service password rejected after all reset");
        const auto logsBefore=data.adminLogs().size();
        rejects([&]{admin.resetPassword(10002,qq,"");},"empty reset password rejected");
        rejects([&]{admin.resetPassword(10002,ServiceHsx::WeiShang,"x");},"unopened service reset rejected");
        expect(data.adminLogs().size()==logsBefore&&data.account(10002,qq).verify("All-new-2026"),"invalid reset preserves password and audit");
        rejects([&]{admin.changePassword("wrong","admin-new-2026");},"wrong original administrator password rejected");
        rejects([&]{admin.changePassword("admin123456","short");},"short administrator password rejected atomically");
        admin.changePassword("admin123456","admin-new-2026");admin.logout();
        rejects([&]{admin.login("admin","admin123456");},"old administrator password rejected after change");admin.login("admin","admin-new-2026");
        const auto hash1=PasswordHsx::hash("same-password"),hash2=PasswordHsx::hash("same-password");
        expect(hash1!=hash2&&PasswordHsx::verify("same-password",hash1)&&PasswordHsx::verify("same-password",hash2),"random salt and correct hash verification");
        expect(!PasswordHsx::verify("wrong",hash1)&&!PasswordHsx::verify("same-password","broken"),"hash rejects wrong password and malformed digest");
        // Independent PBKDF2-HMAC-SHA256 vector (100000 rounds, salt 00..0f) written by the 5.6 format.
        const std::string legacy="pbkdf2-sha256:100000:000102030405060708090a0b0c0d0e0f:9d753e9b3b0729bad2bd2b85f88a394b4d3205d074d9486a089568b794eb2f43";
        expect(PasswordHsx::valid(legacy)&&PasswordHsx::verify("legacy-password-2026",legacy)&&!PasswordHsx::verify("legacy-password-2025",legacy),"legacy 100000-round digest still verifies");
        expect(PasswordHsx::outdated(legacy)&&!PasswordHsx::outdated(hash1)&&hash1.rfind("pbkdf2-sha256:600000:",0)==0,"new digests use 600000 rounds and legacy ones are marked for rehash");
        expect(!PasswordHsx::valid("pbkdf2-sha256:200000:"+legacy.substr(21))&&!PasswordHsx::outdated("broken"),"unsupported round counts are not accepted");
        rejects([]{PasswordHsx::hash("short-pass",PasswordHsx::RemoteMinimum);},"server account passwords need 12 bytes");
        admin.setLoginAllowed(10004,false);const auto file=output/"state.txt";StorageHsx::save(data,file);const auto saved=contents(file);
        expect(saved.rfind("TENCENT 4\n",0)==0,"administrator state saved in version4");
        expect(saved.find("admin-new-2026")==std::string::npos&&saved.find("admin123456")==std::string::npos,"administrator password is never stored as plain text");
        bool cleanAudit=true;for(const auto& entry:data.adminLogs())cleanAudit=cleanAudit&&entry.second.action().find("All-new-2026")==std::string::npos&&entry.second.action().find("QQ-new-2026")==std::string::npos;
        expect(cleanAudit,"audit does not contain password");
        DataStoreHsx restored;StorageHsx::load(restored,file,false);AdminManagerHsx restoredAdmin(restored);AccountManagerHsx restoredAccounts(restored);
        expect(!restoredAdmin.active()&&!restored.user(10002).online(),"user and administrator sessions never persist");
        expect(!restored.user(10004).loginAllowed()&&restored.adminLogs().size()==data.adminLogs().size(),"disabled status and logs survive restart");
        restoredAdmin.initialize();rejects([&]{restoredAdmin.login("admin","admin123456");},"restart initialization preserves changed credential");restoredAdmin.login("admin","admin-new-2026");
        restoredAccounts.login(10002,wc,"All-new-2026");expect(restored.user(10002).online(),"reset user password survives restart");
        rejects([&]{restoredAccounts.login(10004,qq,"123456");},"disabled status enforced after restart");
        const auto bad=output/"bad.txt";
        auto rejectFile=[&](const std::string& content,const char* label){std::ofstream(bad)<<content;rejects([&]{StorageHsx::load(restored,bad,false);},label);expect(restoredAdmin.active()&&restored.users().size()==4&&contents(bad)==content,"failed load preserves memory and disk");};
        const auto end=saved.find("END ");
        rejectFile(saved.substr(0,end)+"STATUS 10004 1\n"+saved.substr(end),"duplicate account status refused");
        const auto configStart=saved.find("ADMINCONFIG "),configEnd=saved.find('\n',configStart);
        rejectFile(saved.substr(0,end)+saved.substr(configStart,configEnd-configStart+1)+saved.substr(end),"duplicate administrator config refused");
        rejectFile(saved.substr(0,configStart)+"ADMINCONFIG \"admin\" \"bad-digest\"\n"+saved.substr(configEnd+1),"malformed administrator digest refused");
        rejectFile(saved.substr(0,end)+"END 4 7 2 6 999\n","wrong log count refused");
        rejectFile(saved.substr(0,end),"truncated administrator archive refused");
        DataStoreHsx old;DemoDataHsx::create(old);const auto oldFile=output/"v3.txt";StorageHsx::save(old,oldFile);
        DataStoreHsx migrated;StorageHsx::load(migrated,oldFile,false);AdminManagerHsx upgrade(migrated);upgrade.initialize();StorageHsx::save(migrated,oldFile);
        size_t services=0;for(const auto& entry:migrated.users())services+=entry.second->services().size();
        expect(migrated.users().size()==4&&services==8&&migrated.groups().size()==7&&migrated.messages().size()==2,"v3 migration preserves users services groups and messages");
        expect(contents(oldFile).rfind("TENCENT 4\n",0)==0&&contents(std::filesystem::path(oldFile.wstring()+L".bak")).rfind("TENCENT 3\n",0)==0,"v3 upgrade retains previous archive backup");
        restoredAdmin.logout();rejects([&]{restoredAdmin.resetPassword(10002,qq,"bad");},"logout revokes administrator privilege");
        std::cout<<"PASS "<<checks<<" administrator checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
