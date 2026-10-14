#include "DemoDataHsx.h"
#include "StorageHsx.h"
#include "AccountManagerHsx.h"
#include "FriendManagerHsx.h"
#include "GroupManagerHsx.h"
#include "MessageManagerHsx.h"
namespace im {
std::filesystem::path DemoDataHsx::fixture(const std::filesystem::path& file) {
    if (!std::filesystem::exists(file)) { DataStoreHsx data; create(data); StorageHsx::save(data,file); }
    return file;
}
void DemoDataHsx::create(DataStoreHsx& data) {
    require(data.users().empty(), "演示数据只能写入空系统");
    AccountManagerHsx account(data); FriendManagerHsx friends(data); GroupManagerHsx groups(data); MessageManagerHsx messages(data);
    account.registerUser(10001, "张三", "2000-05-01", "长春", ServiceHsx::QQ, 10001, "123456", false, "2016-09-01");
    account.registerUser(10002, "李四", "1999-11-11", "北京", ServiceHsx::QQ, 10002, "123456", false, "2017-09-01");
    account.registerUser(10003, "王五", "2001-03-03", "上海", ServiceHsx::WeChat, 40003, "123456", false, "2019-09-01");
    account.registerUser(10004, "赵六", "2000-12-12", "广州", ServiceHsx::QQ, 10004, "123456", false, "2018-09-01");
    for (int id = 10001; id <= 10004; ++id) account.login(id, id == 10003 ? ServiceHsx::WeChat : ServiceHsx::QQ, "123456");
    account.open(10001, ServiceHsx::WeChat, 40001, "张三", "123456"); account.bind(10001, 10001);
    account.open(10001, ServiceHsx::Weibo, 10001, "张三", "123456");
    account.open(10002, ServiceHsx::WeChat, 40002, "李四", "123456"); account.bind(10002, 10002);
    account.open(10004, ServiceHsx::Weibo, 10004, "赵六", "123456");
    const auto link = [&](int from, ServiceHsx source, int to, ServiceHsx destination) {
        const auto id = friends.request(from, source, to, destination, "演示好友"); friends.respond(to, destination, id, true);
    };
    link(10001, ServiceHsx::QQ, 10002, ServiceHsx::QQ);
    link(10001, ServiceHsx::QQ, 10003, ServiceHsx::WeChat);
    link(10001, ServiceHsx::QQ, 10004, ServiceHsx::QQ);
    link(10001, ServiceHsx::WeChat, 10002, ServiceHsx::WeChat);
    link(10001, ServiceHsx::Weibo, 10004, ServiceHsx::Weibo);
    groups.create(10001, ServiceHsx::QQ, "程序设计交流群", GroupModeHsx::QQ);
    groups.invite(10001, ServiceHsx::QQ, 1001, 10002, ServiceHsx::QQ); groups.invite(10001, ServiceHsx::QQ, 1001, 10003, ServiceHsx::WeChat);
    groups.create(10002, ServiceHsx::QQ, "班级群", GroupModeHsx::QQ); groups.invite(10002, ServiceHsx::QQ, 1002, 10001, ServiceHsx::QQ);
    groups.create(10003, ServiceHsx::WeChat, "家庭群", GroupModeHsx::WeChat);
    groups.invite(10003, ServiceHsx::WeChat, 1003, 10001, ServiceHsx::WeChat); groups.invite(10003, ServiceHsx::WeChat, 1003, 10002, ServiceHsx::WeChat);
    groups.create(10001, ServiceHsx::WeChat, "项目组", GroupModeHsx::WeChat); groups.invite(10001, ServiceHsx::WeChat, 1004, 10002, ServiceHsx::WeChat);
    groups.create(10004, ServiceHsx::QQ, "游戏群", GroupModeHsx::QQ); groups.invite(10004, ServiceHsx::QQ, 1005, 10001, ServiceHsx::QQ);
    groups.create(10002, ServiceHsx::WeChat, "自习室", GroupModeHsx::WeChat); groups.invite(10002, ServiceHsx::WeChat, 1006, 10003, ServiceHsx::WeChat);
    groups.create(10001, ServiceHsx::QQ, "算法讨论组", GroupModeHsx::QQ, 1001); groups.invite(10001, ServiceHsx::QQ, 1007, 10002, ServiceHsx::QQ);
    messages.sendPrivate(10002, ServiceHsx::QQ, 10001, "这是一条演示私聊，可收藏、导出或查看回执。");
    messages.sendGroup(10002, ServiceHsx::QQ, 1001, "欢迎使用连续群聊，输入/help查看命令。");
    friends.request(10003, ServiceHsx::WeChat, 10002, ServiceHsx::QQ, "你好，我想加入你的项目交流。");
    for (int id = 10001; id <= 10004; ++id) account.logout(id);
}
}
