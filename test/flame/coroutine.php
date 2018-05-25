<?php
// 进程名称将被设置为：（不含引号）
// "coroutine test (flame-master)"
flame\init("coroutine_test", ["worker"=>1]);
// flame\init("coroutine_test");
flame\go(function() {
	for($i=0;$i<10;++$i) {
		yield flame\time\sleep(50);
		flame\go(function() use($i) {
			yield flame\time\sleep(50);
			echo $i, "\n";
		});
	}
	yield flame\time\sleep(1000);
	echo "done\n";
});

// 运行协程调度（阻塞在此处，实际程序在内部调度循环）
flame\run();
