# 1. 进入构建目录并清理旧数据（确保ID从1开始）
cd /home/project/peer-review-system/build
rm -f disk.img users.dat refcounts.dat *.log *.pdf *.txt

# 2. 创建测试用的虚拟文件
echo "Content of Paper A" > paper_a.pdf
echo "Content of Paper B" > paper_b.pdf
echo "Content of Revision A" > revision_a.pdf
echo "Some random text file" > local_file.txt

# 3. 后台启动服务器
./server > server.log 2>&1 &
SERVER_PID=$!
echo "Server started with PID $SERVER_PID"
sleep 2  # 等待服务器完全启动

# 4. 生成自动化测试指令脚本
cat <<EOF > full_test_script.txt
# === 连接服务器 ===
connect 127.0.0.1 8080
ping

# === 1. 用户注册 (ID 1-5) ===
# 注册不同角色的用户
register author1 pass123 Author
register reviewer1 pass123 Reviewer
register reviewer2 pass123 Reviewer
register editor1 pass123 Editor
register admin1 pass123 Admin

# === 2. 作者流程 (Author1 - ID 1) ===
login author1 pass123
profile "MIT" "AI,Deep Learning"

# 提交两篇论文 (Paper ID 1, 2)
submit "Deep Learning 2026" "Abstract about DL" "AI,DL" paper_a.pdf
submit "Quantum Physics" "Abstract about QP" "Physics,Quantum" paper_b.pdf

# 查看我的论文
mypapers

# 文件操作测试
upload local_file.txt remote_file.txt
list
download remote_file.txt downloaded_file.txt
delete remote_file.txt
logout

# === 3. 编辑流程 - 指派 (Editor1 - ID 4) ===
login editor1 pass123
allpapers

# 将 Paper 1 指派给 Reviewer 1 (ID 2)
assign 1 3

# 尝试自动指派 Paper 2
autoassign 2
logout

# === 4. 评审流程 (Reviewer1 - ID 2) ===
login reviewer1 pass123
toreview

# 提交评审：Paper 1, Accept, 5分
review 1 accept 5 "Excellent work, very innovative."
logout

# === 5. 作者流程 - 上传修订 (Author1 - ID 1) ===
login author1 pass123
# 上传 Paper 1 的修订版
revision 1 revision_a.pdf
logout

# === 6. 编辑流程 - 决策 (Editor1 - ID 4) ===
login editor1 pass123

# 下载论文查看
download_paper 1 downloaded_paper_1.pdf

# 做出最终决定
decision 1 accept

# 查看系统统计
stats
logout

# === 7. 管理员流程 (Admin1 - ID 5) ===
login admin1 pass123

# 修改 Reviewer2 (ID 3) 的角色为 Author
role 3 Author

# 停用 Reviewer2 (ID 3)
deactivate 3

# 触发系统备份
backup
logout

# === 退出 ===
quit
EOF

# 5. 运行客户端并注入测试指令
echo "Running automated tests..."
./client < full_test_script.txt

# 6. 清理服务器进程
kill $SERVER_PID
echo "Test completed."



connect 127.0.0.1 8080
# 注册用户
register author1 pass123 Author
register reviewer1 pass123 Reviewer
register editor1 pass123 Editor

# 作者提交论文
login author1 pass123
submit "Test Paper" "Abstract" "AI" paper.pdf
logout

# 编辑分配审稿人 (假设论文ID为1，审稿人ID为3)
login editor1 pass123
assign 1 3
logout

login reviewer1 pass123

# 【测试点2】审稿人下载待评审论文
# 格式: download_paper <paper_id> <local_filename>
download_paper 1 downloaded_paper.pdf

# 提交评审 (为了产生历史记录)
review 1 accept 5 "Great work!"

# 【测试点3】审稿人查看历史审稿记录 (新功能)
myreviews

logout

login author1 pass123

# 【测试点1】作者查看审稿人意见反馈
mypapers

logout
quit