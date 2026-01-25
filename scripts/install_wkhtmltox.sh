#!/bin/bash
# Install libwkhtmltox for Ubuntu/Debian

set -e

VERSION="0.12.6.1-3"
ARCH=$(dpkg --print-architecture)

# Map architecture names
case "$ARCH" in
    amd64) WKARCH="amd64" ;;
    arm64) WKARCH="arm64" ;;
    *) echo "Unsupported architecture: $ARCH"; exit 1 ;;
esac

# Detect Ubuntu version
. /etc/os-release
UBUNTU_CODENAME=${VERSION_CODENAME:-$(lsb_release -cs)}

# Download URL
URL="https://github.com/wkhtmltopdf/packaging/releases/download/${VERSION}/wkhtmltox_${VERSION}.${UBUNTU_CODENAME}_${WKARCH}.deb"

echo "Downloading wkhtmltox from: $URL"

# Download and install
TEMP_DEB=$(mktemp)
wget -O "$TEMP_DEB" "$URL" || curl -L -o "$TEMP_DEB" "$URL"

echo "Installing wkhtmltox..."
sudo dpkg -i "$TEMP_DEB" || sudo apt-get install -f -y

rm -f "$TEMP_DEB"

echo "Verifying installation..."
if [ -f /usr/include/wkhtmltox/pdf.h ]; then
    echo "libwkhtmltox installed successfully"
else
    echo "Warning: Header files not found at /usr/include/wkhtmltox/"
    echo "You may need to manually install development headers"
fi
