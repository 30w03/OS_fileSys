#!/bin/bash
# Ensure we are in the build directory
if [ -d "build" ]; then
    cd build
fi

rm -f disk.img users.dat

# Create dummy PDF
echo "Dummy PDF Content" > paper.pdf

# Start server
./server -p 8080 -d disk.img > server.log 2>&1 &
SERVER_PID=$!
sleep 2

# Run Client Test
./client <<EOF
connect 127.0.0.1 8080
register author pass Author
register reviewer pass Reviewer
register editor pass Editor
login author pass
submit "Test Paper" "Abstract" "Test" paper.pdf
logout
login editor pass
assign 1 3
logout
login reviewer pass
download_paper 1 downloaded_paper.pdf
review 1 accept 5 "Great paper"
myreviews
logout
login author pass
mypapers
quit
EOF

# Stop server
kill $SERVER_PID
wait $SERVER_PID

# Check download
if [ -f "downloaded_paper.pdf" ]; then
    echo "✅ Paper downloaded successfully"
else
    echo "❌ Paper download failed"
fi

cat server.log
