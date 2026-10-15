# sfx_palette.json 의 태그 목록으로 Source/ProjectM/Audio/NUSFXTags.h/.cpp 를 만든다 (태그를 더하거나 뺀 뒤 돌리고 빌드)
# usage: python make_tags.py
import io, json, os
ROOT = os.path.normpath(os.path.dirname(os.path.abspath(__file__)) + "/../../..").replace("\\", "/") + "/"
rows = json.load(io.open(ROOT + "ArtSource/Audio/SFX/sfx_palette.json", encoding="utf-8"))["sounds"]
tags = []
for r in rows:
    for t in r["tags"]:
        if t not in tags: tags.append(t)
GROUP = {"UI": "버튼·창·알림 같은 화면 조작", "Item": "소지품·장비 조작", "Shop": "상점·공방 결과", "Trade": "1:1 거래", "Social": "채팅·파티",
         "Char": "캐릭터 선택·생성", "Dialogue": "대화창", "Quest": "퀘스트", "Field": "필드 이동·상호작용·성장", "Battle": "전투"}
tree = {}
for t in tags:
    node = tree
    for part in t.split("."):
        node = node.setdefault(part, {})
def emit(node, depth, path, define):
    out = []
    leaves = [k for k, v in node.items() if not v]
    subs = [k for k, v in node.items() if v]
    ind = "\t" * depth
    for k in leaves:
        full = ".".join(path + [k])
        out.append(ind + ('UE_DEFINE_GAMEPLAY_TAG(%s, "NU.SFX.%s")' % (k, full) if define else "UE_DECLARE_GAMEPLAY_TAG_EXTERN(%s);" % k))
    for k in subs:
        if out: out.append("")
        if depth == 2 and not define: out.append(ind + "// " + GROUP[k])
        out.append(ind + "namespace " + k); out.append(ind + "{")
        out += emit(node[k], depth + 1, path + [k], define)
        out.append(ind + "}")
    return out
head = "// Fill out your copyright notice in the Description page of Project Settings.\n\n"
h = head + "#pragma once\n\n#include \"CoreMinimal.h\"\n#include \"NativeGameplayTags.h\"\n\n" \
    "// 효과음 이벤트 태그 (NU.SFX.*). 소리를 낼 곳에서는 UNUSFXSubsystem::Play 에 이 태그만 넘기고,\n" \
    "// 어떤 소리가 나는지는 효과음 표(UNUSFXConfig, NU Audio 설정의 SFXConfigs)가 정한다 --- 표에 없는 태그는 소리가 안 난다.\n" \
    "// NUGameplayTags(전투 담당 소유)를 수정하지 않기 위해 별도 파일에서 NU 네임스페이스를 확장한다\n" \
    "namespace NU\n{\n\tnamespace SFX\n\t{\n" + "\n".join(emit(tree, 2, [], False)) + "\n\t}\n}\n"
c = head + "#include \"Audio/NUSFXTags.h\"\n\nnamespace NU\n{\n\tnamespace SFX\n\t{\n" + "\n".join(emit(tree, 2, [], True)) + "\n\t}\n}\n"
io.open(ROOT + "Source/ProjectM/Audio/NUSFXTags.h", "w", encoding="utf-8", newline="\r\n").write(h)
io.open(ROOT + "Source/ProjectM/Audio/NUSFXTags.cpp", "w", encoding="utf-8", newline="\r\n").write(c)
print("tags", len(tags))
