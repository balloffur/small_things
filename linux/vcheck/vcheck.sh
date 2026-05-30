#!/bin/bash

# vcheck - VPS Monitoring and Access Control Utility

# Automatically detect the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
BANNED_FILE="$SCRIPT_DIR/banned_ips.txt"
LOG_FILE="$SCRIPT_DIR/vcheck.log"

COMMAND=$1
TARGET_IP=$2

# Output Colors
BLUE='\e[1;34m'
RED='\e[1;31m'
GREEN='\e[1;32m'
YELLOW='\e[1;33m'
CYAN='\e[1;36m'
NC='\e[0m'

# Ensure state files exist
touch "$BANNED_FILE"
touch "$LOG_FILE"

function log_action {
    local action=$1
    local ip=$2
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] $action : $ip" >> "$LOG_FILE"
}

function show_status {
    echo -e "${BLUE}=== DISK SPACE ===${NC}"
    df -h / | awk 'NR==1 || NR==2'
    echo ""

    echo -e "${BLUE}=== ACTIVE SSH SESSIONS ===${NC}"
    who | awk '{print "User:", $1, "| TTY:", $2, "| Time:", $3, $4, "| IP:", $5}'
    echo ""

    echo -e "${CYAN}=== LISTENING PORTS & SERVICES ===${NC}"
    echo -e "Proto\tLocal Address:Port\tProcess"
    ss -tulpn | awk 'NR>1 {printf "%-7s %-25s %s\n", $1, $5, $7}' | sed 's/users:(("//g' | sed 's/".*//g'
    echo ""

    echo -e "${RED}=== RECENT FAILED LOGINS ===${NC}"
    journalctl -u ssh --since "1 day ago" --no-pager | grep "Failed password" | tail -n 5 | awk '{print $1, $2, $3, "IP:", $(NF-3)}'
    echo ""
}

function show_banned {
    echo -e "${YELLOW}=== BANNED IPs (BLACKHOLE) ===${NC}"
    
    if [ ! -s "$BANNED_FILE" ]; then
        echo "The banned list is empty."
        echo ""
        return
    fi

    printf "%-18s %s\n" "IP Address" "Details / Comment"
    echo "--------------------------------------------------------"
    
    # Read the file line by line
    while IFS='|' read -r ip comment; do
        printf "%-18s %s\n" "$ip" "$comment"
    done < "$BANNED_FILE"
    echo ""
}

function do_ban {
    local ip=$1
    shift # Remove IP from arguments
    local comment=""

    if [[ -z "$ip" ]]; then
        echo -e "${RED}Error: Please provide an IP address to ban.${NC}"
        return 1
    fi

    if grep -q "^$ip|" "$BANNED_FILE"; then
        echo -e "${YELLOW}IP $ip is already in the banned list.${NC}"
        return 1
    fi

    # Parse manual comment if '-c' flag is used
    if [[ "$1" == "-c" ]]; then
        shift # Remove '-c' from arguments
        comment="$*"
    else
        echo -e "Auto-detecting info for ${CYAN}$ip${NC} via whois..."
        # Extract country and organization from whois output
        local country=$(whois "$ip" | grep -i "^country:" | head -n 1 | awk '{print $2}')
        local org=$(whois "$ip" | grep -iE "^(org-name|netname|descr):" | head -n 1 | cut -d':' -f2- | xargs)
        
        if [[ -z "$country" && -z "$org" ]]; then
            comment="[??] Unknown Origin"
        else
            comment="[${country:-??}] ${org:-Unknown Organization}"
        fi
    fi

    echo -e "Routing IP to blackhole... Details: ${YELLOW}$comment${NC}"
    ip route add blackhole "$ip" 2>/dev/null
    
    # Save to local file and log
    echo "$ip|$comment" >> "$BANNED_FILE"
    log_action "BANNED" "$ip"
    
    echo -e "${GREEN}IP $ip successfully banned.${NC}"
}

function do_unban {
    local ip=$1

    if [[ -z "$ip" ]]; then
        echo -e "${RED}Error: Please provide an IP address to unban.${NC}"
        return 1
    fi

    # Find the IP entry in the file
    local entry=$(grep "^$ip|" "$BANNED_FILE")
    
    if [[ -z "$entry" ]]; then
        echo -e "${YELLOW}Notice: IP $ip is not found in the banned list.${NC}"
        return 1
    fi

    # Extract the comment part (everything after the first pipe '|')
    local comment=$(echo "$entry" | cut -d'|' -f2-)
    
    echo -e "IP ${RED}$ip${NC} is currently banned with the following details:"
    echo -e "${CYAN}$comment${NC}"
    
    # Confirmation prompt
    read -p "$(echo -e 'Are you sure you want to unban this IP? [y/N]: ')" confirm

    if [[ "$confirm" =~ ^[Yy]$ ]]; then
        ip route del blackhole "$ip" 2>/dev/null
        sed -i "/^$ip|/d" "$BANNED_FILE"
        log_action "UNBANNED" "$ip"
        echo -e "${GREEN}IP $ip has been unbanned and removed from the list.${NC}"
    else
        echo -e "Unban cancelled."
    fi
}

function do_clearlogs {
    echo -e "${RED}Warning: You are about to delete the entire ban/unban history.${NC}"
    read -p "$(echo -e 'Are you absolutely sure? [y/N]: ')" confirm
    
    if [[ "$confirm" =~ ^[Yy]$ ]]; then
        > "$LOG_FILE"
        echo -e "${GREEN}Logs cleared successfully.${NC}"
    else
        echo -e "Action cancelled."
    fi
}

# Main command router
case "$COMMAND" in
    status)
        show_status
        ;;
    banned)
        show_banned
        ;;
    ban)
        do_ban "$TARGET_IP" "${@:3}"
        ;;
    unban)
        do_unban "$TARGET_IP"
        ;;
    clearlogs)
        do_clearlogs
        ;;
    *)
        echo -e "Usage:"
        echo -e "  vcheck              - Show system status and banned list"
        echo -e "  vcheck ban <IP>     - Ban IP and auto-detect origin via whois"
        echo -e "  vcheck ban <IP> -c  - Ban IP with a custom comment (e.g., -c My custom rule)"
        echo -e "  vcheck unban <IP>   - Review details and unban IP"
        echo -e "  vcheck clearlogs    - Wipe the vcheck.log file"
        echo ""
        show_status
        show_banned
        ;;
esac
