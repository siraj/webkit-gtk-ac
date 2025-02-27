// There are tests for computeStatistics() located in LayoutTests/fast/harness/perftests

// We need access to console.memory for the memory measurements
if (window.internals)
     internals.settings.setMemoryInfoEnabled(true);

var PerfTestRunner = {};

// To make the benchmark results predictable, we replace Math.random with a
// 100% deterministic alternative.
PerfTestRunner.randomSeed = PerfTestRunner.initialRandomSeed = 49734321;

PerfTestRunner.resetRandomSeed = function() {
    PerfTestRunner.randomSeed = PerfTestRunner.initialRandomSeed
}

PerfTestRunner.random = Math.random = function() {
    // Robert Jenkins' 32 bit integer hash function.
    var randomSeed = PerfTestRunner.randomSeed;
    randomSeed = ((randomSeed + 0x7ed55d16) + (randomSeed << 12))  & 0xffffffff;
    randomSeed = ((randomSeed ^ 0xc761c23c) ^ (randomSeed >>> 19)) & 0xffffffff;
    randomSeed = ((randomSeed + 0x165667b1) + (randomSeed << 5))   & 0xffffffff;
    randomSeed = ((randomSeed + 0xd3a2646c) ^ (randomSeed << 9))   & 0xffffffff;
    randomSeed = ((randomSeed + 0xfd7046c5) + (randomSeed << 3))   & 0xffffffff;
    randomSeed = ((randomSeed ^ 0xb55a4f09) ^ (randomSeed >>> 16)) & 0xffffffff;
    PerfTestRunner.randomSeed = randomSeed;
    return (randomSeed & 0xfffffff) / 0x10000000;
};

PerfTestRunner.now = window.performance && window.performance.webkitNow ? function () { return window.performance.webkitNow(); } : Date.now;

PerfTestRunner.log = function (text) {
    if (this._logLines) {
        this._logLines.push(text);
        return;
    }
    if (!document.getElementById("log")) {
        var pre = document.createElement('pre');
        pre.id = 'log';
        document.body.appendChild(pre);
    }
    document.getElementById("log").innerHTML += text + "\n";
    window.scrollTo(0, document.body.height);
}

PerfTestRunner.logInfo = function (text) {
    if (!window.testRunner)
        this.log(text);
}

PerfTestRunner.loadFile = function (path) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", path, false);
    xhr.send(null);
    return xhr.responseText;
}

PerfTestRunner.computeStatistics = function (times, unit) {
    var data = times.slice();

    // Add values from the smallest to the largest to avoid the loss of significance
    data.sort(function(a,b){return a-b;});

    var middle = Math.floor(data.length / 2);
    var result = {
        min: data[0],
        max: data[data.length - 1],
        median: data.length % 2 ? data[middle] : (data[middle - 1] + data[middle]) / 2,
    };

    // Compute the mean and variance using a numerically stable algorithm.
    var squareSum = 0;
    result.values = times;
    result.mean = data[0];
    result.sum = data[0];
    for (var i = 1; i < data.length; ++i) {
        var x = data[i];
        var delta = x - result.mean;
        var sweep = i + 1.0;
        result.mean += delta / sweep;
        result.sum += x;
        squareSum += delta * delta * (i / sweep);
    }
    result.variance = squareSum / data.length;
    result.stdev = Math.sqrt(result.variance);
    result.unit = unit || "ms";

    return result;
}

PerfTestRunner.logStatistics = function (values, unit, title) {
    var statistics = this.computeStatistics(values, unit);
    this.printStatistics(statistics, title);
}

PerfTestRunner.printStatistics = function (statistics, title) {
    this.log("");
    this.log(title);
    if (statistics.values)
        this.log("values " + statistics.values.join(', ') + " " + statistics.unit);
    this.log("avg " + statistics.mean + " " + statistics.unit);
    this.log("median " + statistics.median + " " + statistics.unit);
    this.log("stdev " + statistics.stdev + " " + statistics.unit);
    this.log("min " + statistics.min + " " + statistics.unit);
    this.log("max " + statistics.max + " " + statistics.unit);
}

PerfTestRunner.storeHeapResults = function() {
    if (!window.internals)
        return;
    this._jsHeapResults.push(this.getUsedJSHeap());
    this._mallocHeapResults.push(this.getUsedMallocHeap());
}

PerfTestRunner.getUsedMallocHeap = function() {
    var stats = window.internals.mallocStatistics();
    return stats.committedVMBytes - stats.freeListBytes;
}

PerfTestRunner.getUsedJSHeap = function() {
    return console.memory.usedJSHeapSize;
}

PerfTestRunner.getAndPrintMemoryStatistics = function() {
    if (!window.internals)
        return;
    var jsMemoryStats = PerfTestRunner.computeStatistics([PerfTestRunner.getUsedJSHeap()], "bytes");
    PerfTestRunner.printStatistics(jsMemoryStats, "JS Heap:");

    var mallocMemoryStats = PerfTestRunner.computeStatistics([PerfTestRunner.getUsedMallocHeap()], "bytes");
    PerfTestRunner.printStatistics(mallocMemoryStats, "Malloc:");
}

PerfTestRunner.gc = function () {
    if (window.GCController)
        window.GCController.collect();
    else {
        function gcRec(n) {
            if (n < 1)
                return {};
            var temp = {i: "ab" + i + (i / 100000)};
            temp += "foo";
            gcRec(n-1);
        }
        for (var i = 0; i < 1000; i++)
            gcRec(10);
    }
}

PerfTestRunner._scheduleNextMeasurementOrNotifyDone = function () {
    if (this._completedRuns < this._runCount) {
        this.gc();
        window.setTimeout(function () {
            var measuredValue = PerfTestRunner._runner();
            PerfTestRunner.ignoreWarmUpAndLog(measuredValue);
            PerfTestRunner._scheduleNextMeasurementOrNotifyDone();
        }, 0);
    } else {
        if (this._description)
            this.log("Description: " + this._description);
        this.logStatistics(this._results, this.unit, "Time:");
        if (this._jsHeapResults.length) {
            this.logStatistics(this._jsHeapResults, "bytes", "JS Heap:");
            this.logStatistics(this._mallocHeapResults, "bytes", "Malloc:");
        }
        if (this._logLines) {
            var logLines = this._logLines;
            this._logLines = null;
            var self = this;
            logLines.forEach(function(text) { self.log(text); });
        }
        if (this._test.done)
            this._test.done();
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

PerfTestRunner._measureTimeOnce = function () {
    var start = this.now();
    var returnValue = this._test.run.call(window);
    var end = this.now();

    if (returnValue - 0 === returnValue) {
        if (returnValue <= 0)
            this.log("runFunction returned a non-positive value: " + returnValue);
        return returnValue;
    }

    return end - start;
}

PerfTestRunner.ignoreWarmUpAndLog = function (result) {
    this._completedRuns++;

    var labeledResult = result + " " + this.unit;
    if (this._completedRuns <= 0)
        this.log("Ignoring warm-up run (" + labeledResult + ")");
    else {
        this._results.push(result);
        this.storeHeapResults();
        this.log(labeledResult);
    }
}

PerfTestRunner._start = function(test) {
    this._description = test.description || "";
    this._completedRuns = -1;
    this._callsPerIteration = 1;
    this._test = test;

    this._results = [];
    this._jsHeapResults = [];
    this._mallocHeapResults = [];

    this._logLines = window.testRunner ? [] : null;
    this.log("Running " + this._runCount + " times");
    this._scheduleNextMeasurementOrNotifyDone();
}

PerfTestRunner.measureTime = function (test) {
    this._runCount = test.runCount || 20;
    this.unit = 'ms';
    this._runner = this._measureTimeOnce;
    this._start(test);
}

PerfTestRunner.runPerSecond = function (test) {
    this._runCount = test.runCount || 20; // Only used by tests in fast/harness/perftests
    this.unit = 'runs/s';
    this._runner = this._measureRunsPerSecondOnce;
    this._start(test);
}

PerfTestRunner._measureRunsPerSecondOnce = function () {
    var timeToRun = this._test.timeToRun || 750;
    var totalTime = 0;
    var i = 0;
    var callsPerIteration = this._callsPerIteration;

    if (this._test.setup)
        this._test.setup();

    while (totalTime < timeToRun) {
        totalTime += this._perSecondRunnerIterator(callsPerIteration);
        i += callsPerIteration;
        if (this._completedRuns < 0 && totalTime < 100)
            callsPerIteration = Math.max(10, 2 * callsPerIteration);
    }
    this._callsPerIteration = callsPerIteration;

    return i * 1000 / totalTime;
}

PerfTestRunner._perSecondRunnerIterator = function (callsPerIteration) {
    var startTime = this.now();
    for (var i = 0; i < callsPerIteration; i++)
        this._test.run.call(window);
    return this.now() - startTime;
}

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}
