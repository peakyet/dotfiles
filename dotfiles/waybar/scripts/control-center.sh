#!/bin/sh
# Control Center hover panel for waybar (macOS-style).
# Prints JSON for hover, or shows the stats in a notification with "notify".

action=${1:-}

# CPU %: sample /proc/stat over a quarter second.
cpu_sample() {
	read -r label user nice system idle iowait irq softirq rest < /proc/stat
	used=$(( user + system ))
	total=$(( used + nice + idle + iowait + irq + softirq ))
}
cpu_sample; u1=$used; t1=$total
sleep 0.25
cpu_sample
cpu_pct=$(( 100 * (used - u1) / (total - t1 + 1) ))
read -r load1 load5 load15 rest < /proc/loadavg
load="$load1 $load5 $load15"

# Memory and swap from the same snapshot.
read -r mem swap <<EOF
$(free -h | awk '/^Mem:/ {gsub(/i/,""); mem=$3"/"$2} /^Swap:/ {gsub(/i/,""); print mem, $3"/"$2}')
EOF

# Root filesystem.
disk=$(df -h / | awk 'NR==2 {print $3"/"$2" ("$5")"}')

# CPU temperature: match the package sensor by name, else n/a. The first
# thermal zone is INT3400, a fixed ~20°C dummy on Intel DPTF systems, and
# zone numbers/glob order are not stable across boots.
temp="n/a"
for zone in /sys/class/thermal/thermal_zone*; do
	read -r type 2>/dev/null < "$zone/type" || continue
	[ "$type" = "x86_pkg_temp" ] || continue
	read -r millideg 2>/dev/null < "$zone/temp" || continue
	[ "$millideg" -gt 0 ] 2>/dev/null || continue
	temp="$((millideg / 1000))°C"
	break
done
if [ "$temp" = "n/a" ]; then
	for hw in /sys/class/hwmon/hwmon*; do
		read -r name 2>/dev/null < "$hw/name" || continue
		case "$name" in
		coretemp | k10temp | zenpower) ;;
		*) continue ;;
		esac
		read -r millideg 2>/dev/null < "$hw/temp1_input" || continue
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
