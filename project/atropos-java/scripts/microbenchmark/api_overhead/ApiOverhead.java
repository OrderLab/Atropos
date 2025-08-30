import java.util.ArrayList;
import java.util.Map;

import autocancel.api.AutoCancel;
import autocancel.api.TaskInfo;

public class ApiOverhead {
	public static void main(String[] args) {
		Long totalCreateTime = 0L;
		Long totalExitTime = 0L;
		Long totalUpdateTime = 0L;

		Long beforeCreateTime = 0L;
		Long beforeExitTime = 0L;
		Long afterExitTime = 0L;

		Long beforeUpdateTime = 0L;
		Long afterUpdateTime = 0L;

		Long maxIter = 1000000L;
		Long prefillCancellables = 500L;

        AutoCancel.start(
            (task, request) -> {
                TaskInfo taskInfo = null;
                TaskInfo.RequestInfo requestInfo = null;
				if (request != null) {
					requestInfo = new TaskInfo.RequestInfo(
                	    "/",
                	    Map.of("Agent", new ArrayList()),
                	    Map.of("pretty", "true"),
                	    request.toString()
                	);
				}
                taskInfo = new TaskInfo(
                    task,
                    Long.valueOf(task.hashCode()),
                    -1L,
                    "Search",
                    100L,
                    100L,
                    task instanceof TaskInfo,
					() -> task instanceof TaskInfo,
                    task.toString(),
                    requestInfo
                );
                return taskInfo;
            },
            (task) -> {
                if (task instanceof TaskInfo) {
					// Do nothing for cancel
                }
            }
        );

		try {
			Thread.sleep(1000);
		}
		catch (Exception e) {
			System.out.println(e.toString());
		}

		for (Long i = 0L; i < maxIter; ++i) {
			Object task = new Object();
			beforeCreateTime = System.nanoTime();
			AutoCancel.onTaskCreate(task, new Object());
			beforeExitTime = System.nanoTime();
			AutoCancel.onTaskExit(task);
			afterExitTime = System.nanoTime();

			totalCreateTime += beforeExitTime - beforeCreateTime;
			totalExitTime += afterExitTime - beforeExitTime;
		}

		for (Long i = 0L; i < prefillCancellables; ++i) {
			AutoCancel.onTaskCreate(new Object(), new Object());
		}

		beforeUpdateTime = System.nanoTime();
		for (Long i = 0L; i < maxIter; ++i) {
			AutoCancel.addMemoryUsage("Cache", 100L, 1024L, 512L, 256L);
		}
		afterUpdateTime = System.nanoTime();
		totalUpdateTime += afterUpdateTime - beforeUpdateTime;

		System.out.println(String.format("Finish. Create: %d, Exit: %d, Update: %d", totalCreateTime / maxIter, totalExitTime / maxIter, totalUpdateTime / maxIter));
	}
}

