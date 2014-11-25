@mkdir include
@mkdir include\libetpan
@for /F "delims=" %%a in (build_headers.list) do @copy "..\%%a" include\libetpan
@echo "done" >_headers_depends