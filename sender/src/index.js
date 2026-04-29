// 本文件是 Node Sender 的程序入口，负责 CLI、运行流程、定时发送和退出清理。
const { Command, InvalidArgumentError } = require('commander');
const { createRuntimeConfig } = require('./configTools');
const { collectMetrics } = require('./metrics');
const { encodeMetrics } = require('./protocol');
const { listPorts, openSerialTransport } = require('./serialTransport');
const { createLogger } = require('./logger');

// 解析参数、处理一次性命令，并启动指标发送循环。
async function main() {
  const cliOptions = parseCliArgs(process.argv);

  if (cliOptions.listPorts) {
    await printPorts();
    return;
  }

  const config = createRuntimeConfig(cliOptions);
  const runtimeLogger = createLogger({ verbose: config.verbose });

  await runSender(config, runtimeLogger);
}

// 解析命令行参数，并把数字参数转换为整数。
function parseCliArgs(argv) {
  const program = new Command();

  program
    .name('m5-monitor-sender')
    .description('Send PC CPU and memory metrics to M5StickC Plus over USB serial.')
    .option('--port <path>', 'serial port path')
    .option('--baud <number>', 'serial baud rate', parseInteger)
    .option('--interval <ms>', 'send interval in milliseconds', parseInteger)
    .option('--list-ports', 'list available serial ports and exit')
    .option('--verbose', 'enable debug logs');

  program.parse(argv);
  const options = program.opts();

  return {
    port: options.port,
    baudRate: options.baud,
    intervalMs: options.interval,
    listPorts: Boolean(options.listPorts),
    verbose: options.verbose === true ? true : undefined,
  };
}

// 打开串口，立即发送一次数据，然后按配置间隔持续发送。
async function runSender(config, logger = createLogger(config)) {
  const transport = await openSerialTransport({
    path: config.port,
    baudRate: config.baudRate,
  });
  let timer = null;

  logger.info(`using serial port: ${transport.path}`);
  logger.info(`sender started, interval: ${config.intervalMs}ms`);

  setupShutdownHandlers(transport, logger, () => timer);
  await sendOnce(transport, logger);

  timer = setInterval(() => {
    sendOnce(transport, logger).catch((error) => {
      logger.warn(`send failed, retrying: ${error.message}`);
    });
  }, config.intervalMs);
}

// 采集当前系统指标，编码为项目协议，并写入串口。
async function sendOnce(transport, logger = createLogger()) {
  const metrics = await collectMetrics();
  const line = encodeMetrics(metrics);

  await transport.write(line);
  logger.debug(`write: ${line.trim()}`);
}

// 注册进程退出信号处理，确保退出前关闭串口。
function setupShutdownHandlers(transport, logger = createLogger(), getTimer = () => null) {
  let shuttingDown = false;

  // 执行一次性关闭流程：停止定时器、关闭串口并结束进程。
  async function shutdown(signal) {
    if (shuttingDown) {
      return;
    }

    shuttingDown = true;
    const timer = getTimer();

    if (timer) {
      clearInterval(timer);
    }

    logger.info(`received ${signal}, closing serial port`);

    try {
      await transport.close();
    } catch (error) {
      logger.error(`failed to close serial port: ${error.message}`);
      process.exitCode = 1;
    } finally {
      process.exit();
    }
  }

  process.once('SIGINT', shutdown);
  process.once('SIGTERM', shutdown);
}

// 列出系统可见串口，供用户选择 --port。
async function printPorts() {
  const ports = await listPorts();

  if (ports.length === 0) {
    console.log('No serial ports found.');
    return;
  }

  for (const port of ports) {
    const details = [
      port.manufacturer && `manufacturer=${port.manufacturer}`,
      port.vendorId && `vendorId=${port.vendorId}`,
      port.productId && `productId=${port.productId}`,
    ].filter(Boolean);

    console.log(`${port.path}${details.length > 0 ? ` (${details.join(', ')})` : ''}`);
  }
}

// commander 参数解析器：将字符串参数转换为整数。
function parseInteger(value) {
  const parsed = Number(value);

  if (!Number.isInteger(parsed)) {
    throw new InvalidArgumentError('must be an integer');
  }

  return parsed;
}

if (require.main === module) {
  main().catch((error) => {
    const logger = createLogger();
    logger.error(error.message);
    process.exitCode = 1;
  });
}

module.exports = {
  main,
  parseCliArgs,
  runSender,
  sendOnce,
  setupShutdownHandlers,
};
