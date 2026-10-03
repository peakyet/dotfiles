#!/bin/sh
# Control Center hover panel for waybar (macOS-style).
# Prints JSON: icon in "text", live stats in "tooltip" (shown on hover).

# CPU %: sample /proc/stat over a quarter second.
cpu_line() { awk '/^cpu /{print $2+$4, $2+$3+$4+$5+$6+$7+$8}' /proc/stat; }
set -- $(cpu_line); u1=$1; t1=$2
sleep 0.25
set -- $(cpu_line); u2=$1; t2=$2
cpu_pct=$(( 100 * (u2 - u1) / (t2 - t1 + 1) ))
load=$(cut -d' ' -f1-3 /proc/loadavg)

# Memory and swap.
mem=$(free -h | awk '/^Mem:/ {gsub(/i/,""); print $3"/"$2}')
swap=$(free -h | awk '/^Swap:/ {gsub(/i/,""); print $3"/"$2}')

# Root filesystem.
disk=$(df -h / | awk 'NR==2 {print $3"/"$2" ("$5")"}')

# CPU temperature: match the package sensor by name, else n/a. The first
# thermal zone is INT3400, a fixed ~20°C dummy on Intel DPTF systems, and
# zone numbers/glob order are not stable across boots.
temp="n/a"
for zone in /sys/class/thermal/thermal_zone*; do
	[ "$(cat "$zone/type" 2>/dev/null)" = "x86_pkg_temp" ] || continue
	millideg=$(cat "$zone/temp" 2>/dev/null)
	[ "$millideg" -gt 0 ] 2>/dev/null || continue
	temp="$((millideg / 1000))°C"
	break
done
if [ "$temp" = "n/a" ]; then
	for hw in /sys/class/hwmon/hwmon*; do
		case "$(cat "$hw/name" 2>/dev/null)" in
		coretemp | k10temp | zenpower) ;;
		*) continue ;;
		esac
		millideg=$(cat "$hw/temp1_input" 2>/dev/null)
		[ "$millideg" -gt 0 ] 2>/dev/null || continue
		temp="$((millideg / 1000))°C"
		break
	done
fi

tooltip="<b>CPU</b>  ${cpu_pct}%  (load ${load})\n<b>Mem</b>  ${mem}\n<b>Swap</b>  ${swap}\n<b>Disk /</b>  ${disk}\n<b>Temp</b>  ${temp}"

printf '{"text":"","tooltip":"%s"}\n' "$tooltip"
