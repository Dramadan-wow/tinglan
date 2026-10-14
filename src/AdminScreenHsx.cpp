#include "AdminScreenHsx.h"
#include "AdminManagerHsx.h"
#include "ConsoleHsx.h"
#include "ScopeHsx.h"
#include <iostream>
namespace im {
void AdminScreenHsx::show() {
    AdminManagerHsx admin(ui_.data()); admin.initialize(); ui_.save();
    std::cout << "系统管理员登录（初始账号 admin，初始密码 admin123456）\n";
    const auto name=trim(ui_.text("管理员账号：")), password=ui_.password("管理员密码：");
    admin.login(name,password); ui_.save();
    ScopeHsx end([&] { admin.logout(); if(ui_.user()&&!ui_.data().user(ui_.user()).online())ui_.session(0,ServiceHsx::Count); ui_.save(); });
    ui_.menu("系统管理 · 0登出管理员", {
        {"查看用户与登录状态", [this] {
            for(const auto& [id,user]:ui_.data().users())
                std::cout<<id<<"  "<<user->name()<<" / "<<(user->loginAllowed()?"允许登录":"禁止登录")<<" / "<<(user->online()?"在线":"离线")<<'\n';
            std::cout<<"在线状态仅反映当前程序中的模拟会话。\n"; }},
        {"允许用户登录", [&] { const int id=ui_.id("用户ID："); admin.setLoginAllowed(id,true); std::cout<<"已允许该用户登录。\n"; }},
        {"禁止用户登录", [&] { const int id=ui_.id("用户ID："); if(ui_.confirm("禁止登录并立即登出该用户")){admin.setLoginAllowed(id,false);std::cout<<"已禁止登录并登出该用户。\n";} }},
        {"强制用户登出", [&] { const int id=ui_.id("用户ID："); if(ui_.confirm("强制登出该用户")){admin.forceLogout(id);std::cout<<"用户已登出。\n";} }},
        {"重置用户密码", [&] {
            const int id=ui_.id("用户ID："); ui_.data().user(id);
            const auto type=ui_.confirm("重置全部已开通服务密码")?ServiceHsx::Count:ui_.chooseService(id);
            const auto fresh=ui_.password("新密码："); require(fresh==ui_.password("确认新密码："),"两次新密码不一致");
            admin.resetPassword(id,type,fresh); std::cout<<"密码已重置，该用户需重新登录。\n"; }},
        {"查看登录与操作记录", [this] {
            for(auto it=ui_.data().adminLogs().rbegin();it!=ui_.data().adminLogs().rend();++it) {
                const auto& log=it->second; std::cout<<timeText(log.created())<<" / "<<log.actor()<<" / "<<log.action();
                if(log.target())std::cout<<" / 用户 "<<log.target(); if(log.service()!=ServiceHsx::Count)std::cout<<" / "<<serviceName(log.service()); std::cout<<'\n';
            } }},
        {"修改管理员密码", [&] {
            const auto original=ui_.password("原管理员密码："), fresh=ui_.password("新密码（至少8个字符）：");
            require(fresh==ui_.password("确认新密码："),"两次新密码不一致"); admin.changePassword(original,fresh); std::cout<<"管理员密码已更新。\n"; }}
    }, [&] { admin.requireAdmin(); return true; });
}
}
