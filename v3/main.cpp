/**********************************************************************
* This code uses MPI library to count word frequencies distributed in *
* several files. Manager-worker pattern is used for self-scheduling.  *
* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> *
* ��ʵû�б�Ҫ����Manager-Worker��ģʽ������Ϊ���������Բ�������������.   *
* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> *
* Author: Dawei Wang												  *
* Date: 2017-07-03.												      *
***********************************************************************/

#include "common.h"
#include "manager.h"
#include "worker.h"

int main(int argc, char *argv[]) {
	int numprocess, proc_rank;
	Manager manager;
	// ��ʼ��Manager
	Init_Manager(&manager);

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocess);
	MPI_Comm_rank(MPI_COMM_WORLD, &proc_rank);
	printf("MPI processor %d has started ...\n", proc_rank);

	MPI_Datatype wordcount_t = createWordCountPairType();

	/* Create a seperate workers communicator from MPI_COMM_WORLD. */
	// Ϊworkers��������һ��Communicator����Ϊ��Gather result��ʱ��Manager
	// �ǲ��ܲ���ģ�ֻ����worker communicator��gather�����ٷ��͸�Manager��
	// �ٴ����꣬�������ʵ�����ǲ���Ҫ����Manager-workerģʽ�ģ�����������ʹ��
	// ������΢���ӻ��ˣ�֮���Բ�����Ϊ������
	MPI_Comm worker_comm;
	createWokerComm(MPI_COMM_WORLD, &worker_comm);

	/******************** Manager Part **********************/

	if (proc_rank == manager_rank) {

		const char fileName[] = "data\\filelist.txt";

		// ��ȡ�����ļ�·�����Լ��������
		GetTasks_Manager(&manager, fileName);

		// Broadcast�������������Ϊ����������Ҫ֪����һ��Ϣ���ж��Լ��Ƿ��ֵܷ�����
		BcastTaskCounts_Manager(&manager);

		// һ���Էַ���һ�����񵽸�������
		DispatchTasks1_Manager(&manager, wordcount_t);

		// ��Ӧworker��work request���������δ��������򷢸���Ӧ���̣�
		// ���򷢳��źŸı���һ���̵�״̬����ʾ��������idle��׼������������һ�׶�
		RespondToWorkReq_Manager(&manager, wordcount_t);

		// ��������worker������ShuffleIssend��ϵ��źţ�ע������ֻ��ʾIssend��ִ�У�
		// ��������ʾ�ѷ��أ���������ʾ�ѱ�Recv
		CheckAllCompleteShuffle_Manager(&manager);

		// �����в���Shuffle��ҪRecv�Ľ��̵�rank�б�㲥��ȥ
		BcastShuffleProcs_Manager(&manager);

		// �������н����в���Shuffle�ĵ��ʵĸ����ܺͣ�����ָ��ManagerӦ����ʲôʱ��
		// ����termination signal����ֹ����ǰ�����Ը���ĳЩ���̽���shuffle words
		SumTotalNumOfWordsInShuffle_Manager(&manager);

		// Managerÿ�յ�һ��Recv��ϵ��źţ��ͽ����¼�£����յ����źŸ�������
		// ��һ����õ��ܺ�ʱ���ͱ�ʾ����Issend��ȥ�ĵ��ʶ���������ϣ����Է���
		// termination signal��
		RecordingShuffleWordsRecvd_Manager(&manager);

		// ����termination signal����ֹ����worker���̵�Recvѭ��
		SendTermSigToShuffleRecv_Manager(&manager, wordcount_t);

		// �������ռ�����
		GetFinalResult_Manager(&manager, wordcount_t);
	}

	/******************** Worker Part **********************/

	else {
		Worker worker;
		// ��ʼ��Worker
		Init_Worker(&worker);

		// ��ȡ���������
		BcastTaskCounts_Worker(&worker);

		// ���worker��proc_rank���ܵ��������ҪС��������Է��䵽����
		if (proc_rank <= worker.task_counts) {
			while (true) {
				// ���ղ��������񣬴�����ɺ�������һ����
				RecvAndProcessTasks_Worker(&worker, wordcount_t);

				// ��Manager����work request����
				int Got = RequireWork_Worker();
				if (!Got)
					break;
			}

			// ����Shuffle�׶Σ���ÿ��word���͵�ָ���Ľ���
			ShuffleIssend_Worker(&worker, proc_rank, wordcount_t);

			// ��Shuffle�׶���Ҫ���յ��ʵ����н��̵�rank���б�㲥��ȥ
			BcastShuffleProcs_Worker(&worker);

			// �������н����в���Shuffle�ĵ��ʵĸ����ܺͣ�����ָ��ManagerӦ����ʲôʱ��
			// ����termination signal����ֹ����ǰ�����Ը���ĳЩ���̽���shuffle words
			SumTotalNumOfWordsInShuffle_Worker(&worker);

			// ����ѭ������Shuffle words������յ�����Ϣ����Manager�����˳�ѭ��ֹͣ����
			ShuffleRecv_Worker(&worker, proc_rank, wordcount_t);

			// Wait���е��첽����Issend
			ShuffleWaitall_Worker(&worker);

			// Gather���յ�result��worker_comm��rank 0�Ľ���
			GatherResultInWorkerComm_Worker(&worker, worker_comm, wordcount_t);

			// �����ս�����͸�Manager
			SendResultToManager_Worker(&worker, worker_comm, wordcount_t);
		}

		// ���worker��proc_rank���ܵ��������Ҫ�����䲻���ܷ��䵽����
		// �����Ҫ����ر��˳������ռ䣬Ҳ��Finalize()
		else {
			MPI_Finalize();
		}
	}

	MPI_Type_free(&wordcount_t);
	MPI_Finalize();

	return 0;
}