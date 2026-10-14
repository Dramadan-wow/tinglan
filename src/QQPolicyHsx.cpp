#include "QQPolicyHsx.h"
namespace im {
GroupModeHsx QQPolicyHsx::mode() const { return GroupModeHsx::QQ; }
bool QQPolicyHsx::applications() const { return true; }
bool QQPolicyHsx::subgroups() const { return true; }
bool QQPolicyHsx::administrators() const { return true; }
std::string QQPolicyHsx::description() const { return "申请入群需审批 / 成员可邀请 / 群主与管理员管理 / 可建讨论组"; }
}
