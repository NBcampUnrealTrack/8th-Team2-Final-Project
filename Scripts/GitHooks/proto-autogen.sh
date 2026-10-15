#!/bin/sh
# ProjectM git hook: keeps the TurboLink gRPC code in step with Protos/*.proto.
#
# The generated code lives in the Plugins/TurboLink submodule and git never
# rebuilds it, so a pull that brings in a proto change leaves it stale until
# Scripts/GenerateProto.ps1 runs - the build then breaks with C2039/C2065 in
# EmberServerManager.cpp and friends. This runs the generator right after git
# has updated the working tree.
#
# Scripts/InstallGitHooks.ps1 adds one line calling this to .git/hooks/<hook>:
#   post-merge     git pull, git merge
#   post-rewrite   git pull --rebase (once, when the rebase finishes)
#   post-checkout  branch switch
#   post-commit    the merge commit that finishes a pull with conflicts
#
# Staleness is judged by content, not by what the update touched: a fingerprint
# of the generator inputs and outputs is recorded after each successful run, so
# a failed run is retried next time and a submodule reset of the generated files
# is caught as well. Nothing is regenerated while the fingerprint still matches.
#
# Never fails the git command that triggered it: every path exits 0.

hook=$1
shift

module=Plugins/TurboLink/Source/TurboLinkGrpc
protoc=Plugins/TurboLink/Source/ThirdParty/protobuf/bin/protoc.exe

root=$(git rev-parse --show-toplevel 2>/dev/null) || exit 0
cd "$root" || exit 0
git_dir=$(git rev-parse --git-dir 2>/dev/null) || exit 0
stamp=$git_dir/projectm-protogen.stamp

in_rebase() {
	[ -d "$git_dir/rebase-merge" ] || [ -d "$git_dir/rebase-apply" ]
}

case $hook in
post-checkout)
	# A rebase checks out its base first; post-rewrite sees the end result.
	in_rebase && exit 0
	;;
post-rewrite)
	# "amend" only rewrites a local commit; "rebase" can bring in upstream protos.
	[ "$1" = rebase ] || exit 0
	;;
post-commit)
	# Only a merge commit can bring in someone else's proto change.
	in_rebase && exit 0
	git rev-parse -q --verify HEAD^2 >/dev/null || exit 0
	;;
esac

# Submodule not checked out here (e.g. a fresh worktree): nothing to keep in sync.
[ -d "$module" ] || exit 0

# Generator inputs (protos, generator script, pinned tool versions) and outputs.
fingerprint() {
	git hash-object --no-filters -- \
		Protos/*.proto \
		Scripts/GenerateProto.ps1 \
		Scripts/TurboLink/turbolink.lock.json \
		"$module/.projectm-generated-files.json" \
		"$module"/Private/pb/* \
		"$module"/Private/S*/* \
		"$module"/Public/S*/* 2>/dev/null |
		git hash-object --stdin
}

current=$(fingerprint)
recorded=
[ -f "$stamp" ] && read -r recorded <"$stamp"
[ "$current" = "$recorded" ] && exit 0

command -v powershell.exe >/dev/null 2>&1 || exit 0
if [ ! -f "$protoc" ]; then
	printf '%s\n' "[proto] TurboLink SDK 가 없어 생성 코드를 확인하지 못했습니다. Scripts\Setup.bat 을 먼저 실행하세요." >&2
	exit 0
fi

printf '%s\n' "[proto] proto 생성 코드 확인 중 (Scripts/GenerateProto.ps1) ..."
if powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File Scripts/GenerateProto.ps1 </dev/null; then
	after=$(fingerprint)
	printf '%s\n' "$after" >"$stamp"
	if [ "$after" != "$current" ]; then
		printf '%s\n' "[proto] 생성 코드가 갱신됐습니다. 에디터가 켜져 있으면 닫고 ProjectMEditor 를 다시 빌드하세요."
	fi
else
	printf '%s\n' "[proto] 생성 실패. 위 오류를 확인하고 Scripts\GenerateProto.bat 을 직접 실행하세요." >&2
fi
exit 0
