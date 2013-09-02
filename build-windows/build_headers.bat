@mkdir include
@mkdir include\libetpan
@for /R ..\src\ %%x in (*.h) do @copy "%%x" include\libetpan
@for %%x in (*.h) do @copy "%%x" include\libetpan
@echo "done" >_headers_depends