## 预处理轨迹

[基本思路说明](https://github.com/v1otusc/extract_XX_trajectory/blob/master/%E5%A4%84%E7%90%86%E6%B5%81%E7%A8%8B%E4%B8%8E%E5%A4%84%E7%90%86%E7%BB%93%E6%9E%9C%E8%AF%B4%E6%98%8E)

## 生成的文件包括：

1. ××××××××××××××_long_lat.txt: 过滤静止点和插值后的GPS经纬度

2. ××××××××××××××_long_lat_MBR.txt: 包含处理后的记录个数以及记录包含的GPS经纬度信息

3. ××××××××××××××_long_lat_debug1.txt: 对原始行记录进行排序后的信息，不包含时间间隔

4. ××××××××××××××_long_lat_debug2.txt: 对原始行记录进行排序后并去除静止点之后的信息，包含了时间间隔

## ~~TODO~~

使用C++中的线程池进行加速 √
