#!/bin/bash
# screenshot-pid.sh — Screenshot a window by PID (KDE Wayland)

pid="$1"
output="${2:-screenshot.png}"

if [[ -z "$pid" ]]; then
    echo "Usage: $0 <pid> [output.png]"
    exit 1
fi

# Get the process name to help disambiguate windows
proc_name=$(ps -p "$pid" -o comm= 2>/dev/null)
if [[ -z "$proc_name" ]]; then
    echo "No process found for PID $pid"
    exit 1
fi

# Find windows for this PID, prefer one whose title matches the process name
best_wid=""
for wid in $(kdotool search --pid "$pid"); do
    name=$(kdotool getwindowname "$wid" 2>/dev/null)
    if [[ "$name" == "$proc_name" ]]; then
        best_wid="$wid"
        break
    fi
    # Fallback: keep the last one (often the main window)
    best_wid="$wid"
done

if [[ -z "$best_wid" ]]; then
    echo "No window found for PID $pid ($proc_name)"
    exit 1
fi

kdotool windowactivate "$best_wid"
sleep 0.3
spectacle -a -b -n -o "$output" 2>/dev/null
echo "Saved to $output"
