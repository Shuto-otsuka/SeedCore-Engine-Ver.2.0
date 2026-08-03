#pragma once
#include <FoundationEngine/JobSystem/WorkerCommon.h>

/**
* [EN]
* Forward declarations for every job-system type, shared so headers can
* reference each other by pointer/reference without pulling in the
* whole job-system include graph. See each type's own header for its
* actual documentation.
*
* ---------------------------------------------------------------------
*
* [JP]
* ジョブシステムの全型に対する前方宣言。各ヘッダがジョブシステムの
* include グラフ全体を引き込まずに、ポインタ/参照で互いを参照できる
* ようにするために共有される。実際のドキュメントは各型自身のヘッダを参照。
*/
namespace SeedCore
{
	class JobNode;
	class JobGraph;
	class FlowBuilder;
	class Semaphore;
	class JobSubflow;
	class JobPreemptiveRuntime;
	class JobNonpreemptiveRuntime;
	class JobTask;
	class JobTaskView;
	class JobTaskflow;
	class JobTopology;
	class JobExecutor;
	class JobWorker;
	class JobWorkerView;

	template <typename T>
	class JobFuture;

	class TaskParams;
	class DefaultTaskParams;
}