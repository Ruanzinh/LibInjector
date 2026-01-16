#!/system/bin/sh
# Este script cria o instalador final em Base64 para você

INJECTOR_BIN="injetor" # Nome do seu binário compilado
OUTPUT_SCRIPT="libinjector.sh"

if [ ! -f "$INJECTOR_BIN" ]; then
    echo "[!] Error: Binary '$INJECTOR_BIN' not found."
    exit 1
fi

echo "[*] Generating Base64 (it may take some seconds)..."
B64_DATA=$(base64 "$INJECTOR_BIN" | tr -d '\n')

echo "[*] Creating final script..."
cat <<'EOF' > "$OUTPUT_SCRIPT"
# TELEGRAM: "@Papagaio_Verdadeiro"
# BY RUANZINH

# Set ur libname here
LIB_NAME="libTool.so"

# Lib Path
ORIGIN_PATH="/sdcard/DCIM/$LIB_NAME"

# DONT MOD THIS SHIT
TEMP_LIB_PATH="/data/local/tmp/$LIB_NAME"
TEMP_BIN="/data/local/tmp/injetor_bin_$RANDOM"

# Colors
GREEN='\033[0;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m'
PURPLE='\033[0;35m'

# Payload Base64
PAYLOAD="__PAYLOAD__"

clear
echo -e "${CYAN}=== Lib Injector via PTRACE ===${NC}"

echo -e "${GREEN}[TELEGRAM]: @Papagaio_Verdadeiro${NC}"

if [ "$(id -u)" -ne 0 ]; then
    echo -e "${RED}[!] ROOT Access Required.${NC}"
    exit 1
fi

trap 'setenforce 1; rm -f "$TEMP_BIN"' EXIT

setenforce 0
echo -e "${PURPLE}[*] SELinux set to Permissive.${NC}"

echo -e "${CYAN}[*] Extracting injector...${NC}"
echo "$PAYLOAD" | base64 -d > "$TEMP_BIN"
chmod 777 "$TEMP_BIN"

if [ ! -s "$TEMP_BIN" ]; then
    echo -e "${RED}[!] Critical error in extraction.${NC}"
    setenforce 1
    echo -e "${PURPLE}[*] SELinux set back to Enforcing.${NC}"
    rm "$TEMP_BIN"
    exit 1
fi

echo "CURRENT PATH: ${ORIGIN_PATH}\nTARGET LIB: ${LIB_NAME}"

echo -e "${GREEN}[?] Game Package (ex: com.shootergamesonline.blockstrike):${NC}"
echo -n "> "
read PACKAGE_NAME

if [ -z "$PACKAGE_NAME" ]; then
    echo -e "${RED}[!] Canceled.${NC}"
    setenforce 1
    echo -e "${PURPLE}[*] SELinux set back to Enforcing.${NC}"
    rm "$TEMP_BIN"
    exit 1
fi

echo -e "${CYAN}[*] Preparing...${NC}"

if [ -f "$ORIGIN_PATH" ]; then
    cp "$ORIGIN_PATH" "$TEMP_LIB_PATH"
    chmod 777 "$TEMP_LIB_PATH"
else
    echo -e "${RED}[!] Lib not found. ${NC}"
    setenforce 1
    echo -e "${PURPLE}[*] SELinux set back to Enforcing.${NC}"
    rm "$TEMP_BIN"
    exit 1
fi

echo -e "${CYAN}[*] Starting game...${NC}"

ACTIVITY=$(cmd package resolve-activity --brief "$PACKAGE_NAME" | tail -n 1)

if [ -z "$ACTIVITY" ]; then
    echo -e "${RED}[!] Unable to resolve main activity.${NC}"
    exit 1
fi

am start -n "$ACTIVITY" > /dev/null 2>&1
sleep 3

PID=$(pidof -s "$PACKAGE_NAME")

if [ -z "$PID" ]; then
    echo -e "${RED}[!] Game closed.${NC}"
    setenforce 1
    echo -e "${PURPLE}[*] SELinux set back to Enforcing.${NC}"
    rm "$TEMP_BIN"
    exit 1
fi

echo -e "${GREEN}[*] Injecting...${NC}"
"$TEMP_BIN" "$PID" "$TEMP_LIB_PATH"

rm "$TEMP_BIN"
echo -e "${GREEN}[DONE] Success.${NC}"
setenforce 1
echo -e "${PURPLE}[*] SELinux set back to Enforcing.${NC}"
EOF
sed -i "s|__PAYLOAD__|$B64_DATA|" "$OUTPUT_SCRIPT"

chmod +x "$OUTPUT_SCRIPT"
echo "[OK] File '$OUTPUT_SCRIPT' created successfully!"