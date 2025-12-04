#!/usr/bin/env bash
# server_setup_nosudo.sh
#
# One-shot script to prepare a non-root (no sudo) Ubuntu user for the
# SAVNET IPFIX demo. It will:
# - create a bare git repo under ~/repos/pmacct.git
# - create a working checkout under ~/pmacct
# - install (download) Go into $HOME/.local/go if `go` is not present
# - clone go-ipfix into ~/pmacct/third_party and attempt to build a collector
# - start the built collector in background (nohup) and write logs to ~/pmacct/collector.log
#
# Usage (run as the target user, not root):
#   bash server_setup_nosudo.sh [<server_repo_path>] [<go_version>]
# Example:
#   bash server_setup_nosudo.sh ~/pmacct 1.20.9
#
set -euo pipefail

TARGET_REPO_PATH=${1:-${HOME}/pmacct}
GO_VERSION=${2:-1.20.9}

echo "Server setup starting"
echo "Target repo path: ${TARGET_REPO_PATH}"
echo "Go version: ${GO_VERSION}"


REPOS_DIR=${HOME}/repos
BARE_REPO=${REPOS_DIR}/pmacct.git
WORK_TREE=${TARGET_REPO_PATH}

mkdir -p "${REPOS_DIR}"

# If the user has already placed the project at ${WORK_TREE}, use it directly.
if [ -d "${WORK_TREE}" ] && [ -d "${WORK_TREE}/.git" ]; then
  echo "Detected existing work tree at ${WORK_TREE}; using it (no bare repo will be created)."
else
  echo "No existing work tree detected at ${WORK_TREE}; will create bare repo and a working clone."
  if [ ! -d "${BARE_REPO}" ]; then
    echo "Creating bare repository: ${BARE_REPO}"
    git init --bare "${BARE_REPO}"
  else
    echo "Bare repo already exists: ${BARE_REPO}"
  fi

  # Setup post-receive hook to checkout to the work tree
  HOOK_FILE="${BARE_REPO}/hooks/post-receive"
  if [ ! -f "${HOOK_FILE}" ]; then
    cat > "${HOOK_FILE}" <<'EOF'
#!/bin/sh
GIT_WORK_TREE="$HOME/pmacct" git checkout -f
EOF
    chmod +x "${HOOK_FILE}"
    echo "post-receive hook installed"
  else
    echo "post-receive hook already present"
  fi

  # Ensure work tree exists (clone if empty)
  if [ ! -d "${WORK_TREE}" ] || [ -z "$(ls -A "${WORK_TREE}" 2>/dev/null)" ]; then
    echo "Creating work tree at ${WORK_TREE} by cloning bare repo"
    git clone "${BARE_REPO}" "${WORK_TREE}"
  else
    echo "Work tree exists: ${WORK_TREE}"
  fi
fi

cd "${WORK_TREE}"

echo "Preparing third_party directory and go-ipfix"
mkdir -p third_party
cd third_party
if [ ! -d go-ipfix ]; then
  echo "Cloning go-ipfix into third_party"
  git clone https://github.com/vmware/go-ipfix.git || true
else
  echo "go-ipfix already present"
fi

# Ensure Go toolchain exists; if not, download into $HOME/.local/go
if command -v go >/dev/null 2>&1; then
  echo "Found go: $(go version)"
  GO_BIN=$(command -v go)
else
  echo "go not found, downloading Go ${GO_VERSION} into ${HOME}/.local/go"
  mkdir -p ${HOME}/.local
  cd ${HOME}/.local
  TAR=go${GO_VERSION}.linux-amd64.tar.gz
  if [ ! -f "${TAR}" ]; then
    wget https://go.dev/dl/${TAR}
  fi
  tar -xzf ${TAR}
  export GOROOT=${HOME}/.local/go
  export PATH=${GOROOT}/bin:${PATH}
  echo "Temporary PATH updated for this session: ${GOROOT}/bin"
fi

# Build collector: try candidate example paths
cd "${WORK_TREE}/third_party/go-ipfix" || exit 0
echo "Attempting to build a collector from go-ipfix"
set +e
BUILD_OK=0
for pkg in ./examples/collector ./cmd/collector ./collector ./cmd/examples/collector; do
  if [ -d "$pkg" ]; then
    echo "Trying to build package: $pkg"
    (cd "$pkg" && go build -o "$WORK_TREE/collector" .) && BUILD_OK=1 && break
  fi
done
set -e

if [ "$BUILD_OK" -eq 1 ]; then
  echo "Collector built at ${WORK_TREE}/collector"
else
  echo "Could not find/build collector example automatically. Please inspect third_party/go-ipfix and build manually if necessary."
fi

# Start collector in background if binary exists
if [ -x "${WORK_TREE}/collector" ]; then
  echo "Starting collector in background (nohup)"
  nohup "${WORK_TREE}/collector" --listen :4739 > "${WORK_TREE}/collector.log" 2>&1 &
  sleep 1
  echo "Collector started (logs: ${WORK_TREE}/collector.log)"
else
  echo "Collector binary not present; skipping run"
fi

echo "Server setup completed. Next steps (on your local machine):"
echo "  1) Add remote in your local repo:" 
echo "     git remote add origin ${USER}@<server>:${BARE_REPO}"
echo "  2) Push your branch: git push -u origin main"
echo "After push, the work tree ${WORK_TREE} will be updated automatically by the post-receive hook."

echo "You can view collector logs at: ${WORK_TREE}/collector.log"

exit 0
