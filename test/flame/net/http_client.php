<?php
flame\init("http_client_test");

flame\go(function() {

	$cli = new flame\net\http\client([
		"debug"=>true, // 调试开关
	]);
	//
	$req = new flame\net\http\client_request("https://www.baidu.com",["arg1"=>"123","arg2"=>"456"]);
	$res = yield $cli->exec($req);
	var_dump($res);



	$req = new flame\net\http\client_request();
	$req->method  = "GET";
	$req->header  = array("Accept"=>"123", "test"=>"Test");
	$req->url     = "http://www.baidu.com";
	$req->timeout = 1000;
	$req->header["Accept"] = "123";
	$req->body = ["arg1"=>"asd","arg2"=>"111qwe"];
	$res = yield $cli->exec($req);
	var_dump($res);

	$res = yield flame\net\http\post("http://www.baidu.com", array("key"=>"123","value"=>"456"));
	var_dump($res);

});

flame\run();