cd /root/proj/filesys/current
docker build -t filesys .
# 获取当前的日时分秒
datetime=$(date "+%d%H%M")
docker tag filesys csjiangyj/filesys:$datetime
docker tag csjiangyj/filesys:$datetime csjiangyj/filesys:latest
docker push csjiangyj/filesys:latetest