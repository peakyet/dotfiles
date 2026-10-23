#!/bin/sh
# Control Center hover panel for waybar (macOS-style).
# Prints JSON for hover, or shows the stats in a notification with "notify".

action=${1:-}

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

# The tooltip takes the CPU's state colour: calm below 50%, warm from 50%,
# hot from 80%. GTK builds one tooltip widget for the whole bar and styles
# every tooltip from a single `tooltip` CSS node, so a module cannot reach it
# from style.css; the label does honour Pango markup, so the span is the way
# in. These are Apple's accessible system colours rather than the bar's
# brighter mac_green/mac_orange/mac_red, which are icon colours and fall to
# ~2:1 contrast as 12px text on the near-white tooltip.
if [ "$cpu_pct" -ge 80 ]; then
	colour='#D70015' # red
elif [ "$cpu_pct" -ge 50 ]; then
	colour='#C93400' # orange
else
	colour='#248A3D' # green
fi

stats="<b>CPU</b>  ${cpu_pct}%  (load ${load})\n<b>Mem</b>  ${mem}\n<b>Swap</b>  ${swap}\n<b>Disk /</b>  ${disk}\n<b>Temp</b>  ${temp}"
tooltip="<span foreground='${colour}'>${stats}</span>"
# The icon takes the same colour, so the bar itself signals the load; the
# module renders its text as markup, which also beats the CSS colour.
icon="<span foreground='${colour}'></span>"

if [ "$action" = "notify" ]; then
	notify-send --app-name="Control Center" --expire-time=5000 "System status" "$(printf '%b' "$stats")"
else
	printf '{"text":"%s","tooltip":"%s"}\n' "$icon" "$tooltip"
fi
