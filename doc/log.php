<?php

/**
 * 日志模块, 可通过框架配置设置日志等级以过滤日志输出;
 * @see flame\init()
 * 注意：单进程模式不支持设置输出目标文件，固定输出到 STDOUT / STDERR (由错误等级决定)
 */
namespace flame\log;
/**
 * 记录 TRACE 级别日志, 自动进行 JSON 形式的序列化;
 */
function trace($x/*, $y, $z  ... */) {}
/**
 * 记录 DEBUG 级别日志, 自动进行 JSON 形式的序列化;
 */
function debug($x/*, $y, $z  ... */) {}
/**
 * 记录 INFO 级别日志, 自动进行 JSON 形式的序列化;
 */
function info($x/*, $y, $z  ... */) {}
/**
 * 记录 WARNING 级别日志, 自动进行 JSON 形式的序列化;
 */
function warn($x/*, $y, $z  ... */) {}
/**
 * 记录 WARNING 级别日志, 自动进行 JSON 形式的序列化;
 */
function warning($x/*, $y, $z  ... */) {}
/**
 * 记录 ERROR 级别日志, 自动进行 JSON 形式的序列化;
 */
function error($x/*, $y, $z  ... */) {}
/**
 * 同 fatal()
 * @see fatal()
 */
function fail($x/*, $y, $z  ... */) {}
/**
 * 记录 FATAL 级别日志, 自动进行 JSON 形式的序列化;
 */
function fatal($x/*, $y, $z  ... */) {}