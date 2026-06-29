## How to Run


create the shared library
```
gcc main.c -o hook.so -shared -fPIC -g
```



switch to your FTL binary and then:
```
LD_PRELOAD=/home/neal/Github/ftl-game-hijacking/playground/ftl/hook.so ./FTL.amd64 

```
