#include "AppearanceHsx.h"
namespace im {
namespace {
QJsonObject style(const char* id,const char* name,int tier,const char* a,const char* b,const char* ink,const char* effect="plain") {
    return {{"id",id},{"name",name},{"tier",tier},{"a",a},{"b",b},{"ink",ink},{"effect",effect}};
}
}
QJsonArray AppearanceHsx::catalog(const QString& kind) {
    if(kind=="theme")return {style("jade","听澜青",0,"#356552","#eef5e9","#173e34"),style("ocean","晴空蓝",0,"#3977b7","#eef5fd","#173b5f"),style("lilac","鸢尾紫",0,"#8164b3","#f5f0fb","#36264f"),style("rose","落樱粉",0,"#b66986","#fff0f5","#592e43"),style("amber","日落橙",0,"#b77b3c","#fff7e9","#533c25"),style("night","星夜靛",0,"#5768ae","#f0f2fc","#252e53")};
    if(kind=="bubble")return {
        style("jade","初见青竹",0,"#dcebd5","#dcebd5","#304d35"),style("sky","晴空来信",0,"#ddecff","#e9f3ff","#274c70"),style("peach","蜜桃心事",0,"#ffe1e8","#fff1ef","#7d4157"),style("lilac","紫藤花语",0,"#e8dffc","#f3edff","#5a467b"),style("sun","橘子汽水",0,"#ffe8ba","#fff3d6","#79522e"),style("paper","素笺留白",0,"#ffffff","#f7f8fc","#47566c"),
        style("mint","薄荷汽泡",1,"#a9f1df","#ddfcef","#216352","dots"),style("sakura","樱花信笺",1,"#f9cbe1","#fce9f5","#824765","flowers"),
        style("candy","糖果星球",2,"#f7c7ff","#bedfff","#63407c","stars"),style("ocean","海盐蓝调",2,"#95d8ff","#d6f4ff","#255b80","waves"),style("sunset","落日橘海",2,"#ffc89b","#ffe2e9","#864c3d","dots"),
        style("aurora","极光流彩",3,"#b3eee2","#d4c4ff","#425b72","aurora"),style("galaxy","银河漫游",3,"#41497e","#7461a3","#ffffff","stars"),
        style("gold","云境鎏金",4,"#ffedb2","#efd4a2","#7d5930","gold"),style("prism","琉璃幻境",4,"#eec7ff","#adf5ed","#574678","prism")};
    if(kind=="frame")return {style("none","自然头像",0,"#ffffff","#ffffff","#ffffff"),style("mint","青芽环",1,"#70cba3","#a8ead2","#ffffff"),style("rose","月汐环",2,"#ed91bd","#b8a3fa","#ffffff"),style("star","星河环",3,"#798be8","#89dfec","#ffffff"),style("gold","云境金环",4,"#d7a74a","#ffe8a4","#ffffff")};
    if(kind=="pet")return {style("flame","暖橙小火人",0,"#ff963b","#ffd56e","#603d29"),style("mint","薄荷精灵",1,"#52c7a8","#bcf2bb","#325b4f"),style("pink","樱粉小火人",2,"#eb80ac","#ffd0e8","#6b3e57"),style("violet","星焰精灵",3,"#9b81ed","#d6c8ff","#4c3e75"),style("gold","鎏金守护者",4,"#e7ad3d","#fff3a9","#694918")};
    return {};
}
QJsonObject AppearanceHsx::item(const QString& kind,const QString& id) { for(const auto& value:catalog(kind))if(value.toObject().value("id")==id)return value.toObject();return {}; }
}
