docker build -t filesys .
# 获取当前的日时分秒
datetime=$(date "+%d%H%M")
docker tag filesys csjiangyj/filesys:$datetime
docker push csjiangyj/filesys:$datetime