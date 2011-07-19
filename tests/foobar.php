<?php
mb_internal_encoding("UTF-8");
#echo mb_decode_mimeheader("=?ISO-2022-JP?B?UmU6IBskQjl1PC9QchsoQg==?=");
echo mb_decode_mimeheader("=?GB2312?B?UmU6IPxcwrmV/g==?=") . "\n";
echo mb_decode_mimeheader("=?GB2312?B?UmU6IPxcwrmV/g==?=") . "\n";
#echo iconv_mime_decode("=?ISO-2022-JP?B?UmU6IBskQjl1PC9QchsoQg==?=", 0, "utf-8");
#echo iconv_mime_decode("=?GB2312?B?UmU6IPxcwrmV/g==?=", 0, "utf-8");
echo mb_decode_mimeheader("=?GB2312?B?nHnUh4bho78=?=") . "\n";
echo mb_decode_mimeheader("=?utf-8?B?5ris6Kmm5ZeO77yf?=") . "\n";
?>
