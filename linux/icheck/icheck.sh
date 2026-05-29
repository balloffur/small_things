#!/bin/bash

# ==========================================
# Инициализация и пути
# ==========================================
CONFIG_DIR="$HOME/.config/icheck"
HELP_FILE="$CONFIG_DIR/help.txt"
CONFIG_FILE="$CONFIG_DIR/config.conf"
DB_FILE="$CONFIG_DIR/database.txt"

# Дефолтные настройки
TIMEOUT=5
CHECK_ALL=0
SHOW_TIME=0
IP_VER=""
PING_VER="ping"

# Режим работы по умолчанию
RUN_MODE="fast"

# Флаги точечного вывода
SHOW_LOCAL=0
SHOW_PUBLIC=0
SHOW_DNS=0
SHOW_PROC=0
SHOW_BL=0
TARGET_PROC=""
CUSTOM_TARGET=""

# Цвета
GREEN="\033[1;32m"
RED="\033[1;31m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
NC="\033[0m"

# ==========================================
# Парсер аргументов
# ==========================================
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -f|-fast) RUN_MODE="fast" ;;
        -m|-more) RUN_MODE="standard" ;;
        -a|-all) RUN_MODE="all"; CHECK_ALL=1 ;;
        -l|-local) RUN_MODE="custom"; SHOW_LOCAL=1 ;;
        -p|-public|-publick) RUN_MODE="custom"; SHOW_PUBLIC=1 ;;
        -d|-dns) RUN_MODE="custom"; SHOW_DNS=1 ;;
        -bl|-blacklist) RUN_MODE="custom"; SHOW_BL=1 ;;
        -proc) RUN_MODE="custom"; SHOW_PROC=1; TARGET_PROC="$2"; shift ;;
        -t|-time) SHOW_TIME=1 ;;
        -timeout) TIMEOUT="$2"; shift ;;
        -4|-ipv4) IP_VER="-4"; PING_VER="ping" ;;
        -6|-ipv6) IP_VER="-6"; PING_VER="ping6" ;;
        -h|-help|--help) cat "$HELP_FILE" 2>/dev/null || echo "Справка не найдена."; exit 0 ;;
        -*) echo -e "${RED}Неизвестный аргумент: $1${NC}"; exit 1 ;;
        *) RUN_MODE="target"; CUSTOM_TARGET="$1" ;; # Аргумент без дефиса
    esac
    shift
done

# Логика режимов
if [ "$RUN_MODE" = "standard" ]; then
    SHOW_LOCAL=1; SHOW_PUBLIC=1; SHOW_DNS=1
elif [ "$RUN_MODE" = "all" ]; then
    SHOW_LOCAL=1; SHOW_PUBLIC=1; SHOW_DNS=1; SHOW_BL=1
fi

# Настройка списков для HTTP/Ping
if [ $CHECK_ALL -eq 1 ]; then
    DEFAULT_PING_TARGETS=("1.1.1.1" "8.8.8.8" "9.9.9.9" "ya.ru" "vk.com" "amazon.com" "github.com")
    DEFAULT_HTTP_TARGETS=("youtube.com" "twitter.com" "instagram.com" "facebook.com" "dzen.ru" "mail.ru" "wikipedia.org")
else
    DEFAULT_PING_TARGETS=("1.1.1.1" "ya.ru")
    DEFAULT_HTTP_TARGETS=("youtube.com" "twitter.com")
fi

# Загрузка конфига пользователя
CUSTOM_PING_TARGETS=""
CUSTOM_HTTP_TARGETS=""
if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
fi

# ==========================================
# Вспомогательные функции
# ==========================================

require_tool() {
    local cmd="$1"
    local apt_pkg="$2"
    if ! command -v "$cmd" &> /dev/null; then
        echo -e "${RED}Ошибка: Утилита '$cmd' не установлена.${NC}"
        echo -e "Для установки выполните: ${YELLOW}sudo apt update && sudo apt install $apt_pkg -y${NC}"
        exit 1
    fi
}

lookup_db() {
    local query="$1"
    if [ -f "$DB_FILE" ]; then
        local result=$(awk -F'|' -v q="$query" '$1 == q {print $2; exit}' "$DB_FILE")
        result=$(echo "$result" | tr -d '\r')
        if [ -n "$result" ]; then
            echo "$result"
            return
        fi
    fi
    echo "Unknown"
}

# ==========================================
# Режим Быстрой Проверки (Fast Mode)
# ==========================================
check_fast() {
    echo -e "\n${CYAN}⚡ [ Быстрый статус ] ⚡${NC}"
    
    local lip=$(hostname -I | awk '{print $1}')
    echo -e "Локальный IP: ${GREEN}${lip:-Отсутствует}${NC}"

    local trace_data=$(curl -s $IP_VER --max-time 2 https://cloudflare.com/cdn-cgi/trace)
    local ip=$(echo "$trace_data" | grep -E "^ip=" | cut -d= -f2)
    local iso=$(echo "$trace_data" | grep -E "^loc=" | cut -d= -f2)

    if [ -n "$ip" ]; then
        local city=$(curl -s $IP_VER --max-time 1 "wttr.in/?format=%l" | sed 's/+/ /g' | awk '{for(i=1;i<=NF;i++) $i=toupper(substr($i,1,1)) tolower(substr($i,2)); print}')
        echo -e "Публичный IP: ${GREEN}$ip${NC} (Локация: ${YELLOW}${city:-Код страны: $iso}${NC})"
    else
        echo -e "Публичный IP: ${RED}Ошибка соединения${NC}"
    fi

    echo -n "Пинг google.com: "
    local ping_out=$($PING_VER -c 1 -W 2 google.com 2>&1)
    if [ $? -eq 0 ]; then
        local p_time=$(echo "$ping_out" | awk -F'time=' '/time=/{print $2}' | awk '{print $1" "$2}')
        local t_str=""; [ $SHOW_TIME -eq 1 ] && t_str=" (${CYAN}${p_time}${NC})"
        echo -e "${GREEN}ОК${NC}$t_str"
    else
        echo -e "${RED}НЕДОСТУПЕН${NC}"
    fi
    echo ""
}

# ==========================================
# Основные функции
# ==========================================

check_custom_target() {
    local clean_target=$(echo "$CUSTOM_TARGET" | sed -E 's|https?://||; s|/.*||')
    echo -e "\n${CYAN}--- [ Анализ цели: $clean_target ] ---${NC}"
    
    local desc=$(lookup_db "$clean_target")
    if [ "$desc" != "Unknown" ]; then
        echo -e "База данных: ${YELLOW}$desc${NC}"
    fi

    echo -n "ICMP (Ping): "
    local ping_out=$($PING_VER -c 1 -W "$TIMEOUT" "$clean_target" 2>&1)
    if [ $? -eq 0 ]; then
        local p_time=$(echo "$ping_out" | awk -F'time=' '/time=/{print $2}' | awk '{print $1" "$2}')
        local t_str=""; [ $SHOW_TIME -eq 1 ] && t_str=" (${CYAN}${p_time}${NC})"
        echo -e "${GREEN}ДОСТУПЕН${NC}$t_str"
    else
        echo -e "${RED}НЕДОСТУПЕН${NC}"
    fi

    echo -n "HTTPS:       "
    local curl_out=$(curl -s $IP_VER -o /dev/null -w "%{http_code}|%{time_total}" --max-time "$TIMEOUT" "https://$clean_target")
    local http_code=$(echo "$curl_out" | cut -d'|' -f1)
    local h_time=$(echo "$curl_out" | cut -d'|' -f2)
    
    if [ "$http_code" != "000" ]; then
        local t_str=""; [ $SHOW_TIME -eq 1 ] && t_str=" (${CYAN}${h_time}с${NC})"
        echo -e "${GREEN}ОТКРЫТ${NC} [HTTP $http_code]$t_str"
    else
        echo -e "${RED}ТАЙМАУТ / БЛОКИРОВКА${NC}"
        echo -n "HTTP (80):   "
        local curl_http=$(curl -s $IP_VER -o /dev/null -w "%{http_code}|%{time_total}" --max-time "$TIMEOUT" "http://$clean_target")
        local http_code_80=$(echo "$curl_http" | cut -d'|' -f1)
        if [ "$http_code_80" != "000" ]; then
            echo -e "${YELLOW}ОТКРЫТ (Без шифрования)${NC} [HTTP $http_code_80]"
        else
            echo -e "${RED}ТАЙМАУТ / БЛОКИРОВКА${NC}"
        fi
    fi
    echo ""
}

check_ping() {
    local targets=($CUSTOM_PING_TARGETS "${DEFAULT_PING_TARGETS[@]}")
    echo -e "\n${CYAN}--- [ Доступность интернета (ICMP) ] ---${NC}"
    
    for target in "${targets[@]}"; do
        echo -n "Пинг $target... "
        local ping_out=$($PING_VER -c 1 -W "$TIMEOUT" "$target" 2>&1)
        if [ $? -eq 0 ]; then
            local p_time=$(echo "$ping_out" | awk -F'time=' '/time=/{print $2}' | awk '{print $1" "$2}')
            local t_str=""; [ $SHOW_TIME -eq 1 ] && t_str=" (${CYAN}${p_time}${NC})"
            echo -e "${GREEN}ОК${NC}$t_str"
        else
            echo -e "${RED}Сбой${NC}"
        fi
    done
}

check_http() {
    local targets=($CUSTOM_HTTP_TARGETS "${DEFAULT_HTTP_TARGETS[@]}")
    echo -e "\n${CYAN}--- [ Доступность сайтов (HTTPS) ] ---${NC}"
    
    for target in "${targets[@]}"; do
        echo -n "Запрос $target... "
        local curl_out=$(curl -s $IP_VER -o /dev/null -w "%{http_code}|%{time_total}" --max-time "$TIMEOUT" "https://$target")
        local http_code=$(echo "$curl_out" | cut -d'|' -f1)
        local h_time=$(echo "$curl_out" | cut -d'|' -f2)
        
        if [ "$http_code" != "000" ]; then
             local t_str=""; [ $SHOW_TIME -eq 1 ] && t_str=" (${CYAN}${h_time}с${NC})"
             echo -e "${GREEN}ОТКРЫТ${NC}$t_str"
        else
             echo -e "${RED}БЛОКИРОВКА / ТАЙМАУТ${NC}"
        fi
    done
}

get_local_ip() {
    echo -e "\n${CYAN}--- [ Локальная сеть ] ---${NC}"
    local ip=$(hostname -I | awk '{print $1}')
    if [ -n "$ip" ]; then
        local desc=$(lookup_db "$ip")
        echo -e "Локальный IP: ${GREEN}$ip${NC} ($desc)"
    else
        echo -e "${RED}Нет локального IP${NC}"
    fi
}

get_public_info() {
    echo -e "\n${CYAN}--- [ Публичная сеть ] ---${NC}"
    
    local trace_data=$(curl -s $IP_VER --max-time "$TIMEOUT" https://cloudflare.com/cdn-cgi/trace)
    local ip=$(echo "$trace_data" | grep -E "^ip=" | cut -d= -f2)
    local iso=$(echo "$trace_data" | grep -E "^loc=" | cut -d= -f2)

    if [ -n "$ip" ]; then
        local loc_name="Код страны: $iso"
        local raw_loc=$(curl -s $IP_VER --max-time "$TIMEOUT" "wttr.in/?format=%l" | sed 's/+/ /g')
        if [ -n "$raw_loc" ]; then
            loc_name=$(echo "$raw_loc" | awk '{for(i=1;i<=NF;i++) $i=toupper(substr($i,1,1)) tolower(substr($i,2)); print}')
        fi
        
        echo -e "Публичный IP: ${GREEN}$ip${NC}"
        echo -e "Локация:      ${YELLOW}$loc_name${NC}"
    else
        echo -e "${RED}Таймаут определения внешнего IP${NC}"
    fi
}

get_dns() {
    echo -e "\n${CYAN}--- [ DNS Серверы ] ---${NC}"
    local dns_list=""
    
    if command -v resolvectl &> /dev/null; then
        dns_list=$(resolvectl dns | awk '{for(i=4;i<=NF;i++) print $i}')
    else
        dns_list=$(grep nameserver /etc/resolv.conf | awk '{print $2}')
    fi

    if [ -n "$dns_list" ]; then
        for dns in $dns_list; do
            local desc=$(lookup_db "$dns")
            echo -e "DNS: ${GREEN}$dns${NC} [${YELLOW}$desc${NC}]"
        done
    else
        echo -e "${RED}DNS серверы не найдены${NC}"
    fi
}

check_proc() {
    echo -e "\n${CYAN}--- [ Мониторинг процесса: $TARGET_PROC ] ---${NC}"
    local pid=$(pgrep -f "$TARGET_PROC" | head -n 1)
    
    if [ -n "$pid" ]; then
        echo -e "Статус: ${GREEN}ЗАПУЩЕН${NC} (PID: $pid)"
        local stats=$(ps -p "$pid" -o %cpu,%mem,etime --no-headers)
        local cpu=$(echo $stats | awk '{print $1}')
        local mem=$(echo $stats | awk '{print $2}')
        local time=$(echo $stats | awk '{print $3}')
        
        echo -e "CPU:    ${YELLOW}$cpu%${NC}"
        echo -e "RAM:    ${YELLOW}$mem%${NC}"
        echo -e "Время:  ${YELLOW}$time${NC}"
    else
        echo -e "Статус: ${RED}НЕ НАЙДЕН${NC}"
    fi
}

check_blacklist() {
    local ip=$(curl -s -4 --max-time "$TIMEOUT" https://cloudflare.com/cdn-cgi/trace | grep -E "^ip=" | cut -d= -f2)
    
    if [ -z "$ip" ]; then
        echo -e "\n${RED}Не удалось получить IP для проверки DNSBL.${NC}"
        return
    fi

    echo -e "\n${CYAN}--- [ Антиспам Базы (DNSBL): $ip ] ---${NC}"
    local rev_ip=$(echo "$ip" | awk -F. '{print $4"."$3"."$2"."$1}')
    
    local bl_servers=("zen.spamhaus.org" "bl.spamcop.net" "b.barracudacentral.org" "cbl.abuseat.org" "dnsbl.sorbs.net")

    for bl in "${bl_servers[@]}"; do
        echo -n "Проверка $bl... "
        if host -t A "$rev_ip.$bl" &>/dev/null; then
            echo -e "${RED}⚠️ В ЧЕРНОМ СПИСКЕ${NC}"
        else
            echo -e "${GREEN}ЧИСТО${NC}"
        fi
    done
}

# ==========================================
# Исполнение программы
# ==========================================

# 1. Зависимости
require_tool "curl" "curl"
require_tool "awk" "gawk"
require_tool "$PING_VER" "iputils-ping"
require_tool "host" "dnsutils"

# 2. Выбор сценария
if [ "$RUN_MODE" = "fast" ]; then
    check_fast
    exit 0
elif [ "$RUN_MODE" = "target" ]; then
    check_custom_target
    exit 0
fi

# 3. Полные и кастомные проверки
[ $SHOW_LOCAL -eq 1 ] && get_local_ip
[ $SHOW_PUBLIC -eq 1 ] && get_public_info
if [ $SHOW_PUBLIC -eq 1 ] || [ "$RUN_MODE" = "standard" ] || [ "$RUN_MODE" = "all" ]; then
    check_ping
    check_http
fi
[ $SHOW_DNS -eq 1 ] && get_dns
[ $SHOW_BL -eq 1 ] && check_blacklist
[ $SHOW_PROC -eq 1 ] && check_proc

echo ""
