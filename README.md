# CodeCraft2020

- main63.cpp : 6+3 版本代码，线下4.8x, 线上2.6x。（线下为1963w数据，下同）
- main43.cpp : 4+3 版本代码，线下5.5x, 线上2.3x。


- test.sh : 批量测试脚本，可用于测试路径、答案等是否正确。用法 : 
        - 添加执行权限：`chmod u+x test.sh`
        - 测试：`./test.sh main.cpp`
        - (可能提示没有权限创建文件，以 sudo 权限运行即可)
    - test_data_xxxx.txt : 数据集，其中xx为环的个数
    - answer_xxxx.txt : 与数据集对应的答案
    - log.txt : 测试日志文件，可用于排查错误
