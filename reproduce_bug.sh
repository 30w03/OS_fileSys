#!/bin/bash
# Ensure we are in the build directory
if [ -d "build" ]; then
    cd build
fi

rm -f disk.img users.dat refcounts.dat *.log *.pdf *.txt

# Create dummy files
echo "Content of Paper A" > paper_a.pdf
echo "Content of Paper B" > paper_b.pdf

# Start server
./server -p 8080 -d disk.img > server.log 2>&1 &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID"
sleep 2

# Run client commands
./client > client_output.txt <<EOF
connect 127.0.0.1 8080
register author1 pass123 Author
register reviewer1 pass123 Reviewer
login author1 pass123
submit "Deep Learning 2026" "Abstract about DL" "AI,DL" paper_a.pdf
submit "Quantum Physics" "Abstract about QP" "Physics,Quantum" paper_b.pdf
logout
login admin admin123
role 3 Reviewer
logout
login author1 pass123
mypapers
logout
login reviewer1 pass123
toreview
logout
login admin admin123
assign 1 3
logout
login reviewer1 pass123
toreview
review 1 accept 5 "Good"
logout
login author1 pass123
mypapers
quit
EOF

# Stop server
kill $SERVER_PID
wait $SERVER_PID

cat client_output.txt
grep "DEBUG" server.log
