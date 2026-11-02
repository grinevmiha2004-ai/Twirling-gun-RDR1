#!/usr/bin/env bash
set -euo pipefail
tools_dir=/workspace/tools
mkdir -p "$tools_dir/apt/lists/partial" "$tools_dir/apt/cache/archives/partial" "$tools_dir/debs" "$tools_dir/root"
cat > "$tools_dir/apt/sources.list" <<'EOF'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
EOF
apt_options=(-o "Dir::Etc::sourcelist=$tools_dir/apt/sources.list" -o Dir::Etc::sourceparts=- -o "Dir::State::lists=$tools_dir/apt/lists" -o "Dir::Cache=$tools_dir/apt/cache")
if [[ -n "${HTTPS_PROXY:-}" ]]; then apt_options+=(-o "Acquire::https::Proxy=$HTTPS_PROXY"); fi
apt-get "${apt_options[@]}" update
cd "$tools_dir/debs"
apt-get "${apt_options[@]}" download g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix-runtime gcc-mingw-w64-base binutils-mingw-w64-x86-64 mingw-w64-x86-64-dev mingw-w64-common 7zip libarchive-tools
for package in ./*.deb; do dpkg-deb -x "$package" "$tools_dir/root"; done
"$tools_dir/root/usr/bin/x86_64-w64-mingw32-g++-posix" --version
