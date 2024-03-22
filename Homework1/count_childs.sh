
# Function to count child processes
count_child_processes() {
    local count=$(ps -eo pid,ppid,comm | grep gtuStudentGrades | wc -l)
    echo -ne "\rChild process count: $count"
}

# Initial count
count_child_processes

# Main loop
prev_count=$(ps -eo pid,ppid,comm | grep gtuStudentGrades | wc -l)
while true; do
    new_count=$(ps -eo pid,ppid,comm | grep gtuStudentGrades | wc -l)
    if [ "$new_count" -ne "$prev_count" ]; then
        count_child_processes
        prev_count=$new_count
    fi
    sleep 0.1  # Adjust the sleep duration as needed
done