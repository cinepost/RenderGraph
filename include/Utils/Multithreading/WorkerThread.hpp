/*
See LICENSE file in root folder
*/
#ifndef ___Utils_WorkerThread_H___
#define ___Utils_WorkerThread_H___

#include "Utils/UtilsPrerequisites.hpp"

#include <Utils/Signal.hpp>

#include <thread>
#include <functional>
#include <atomic>
#include <mutex>

namespace utils
{
	/*!
	\author		Sylvain DOREMUS
	\version	0.9.0
	\date		02/12/2016
	\~english
	\brief		Implementation of a worker thread to place in a thread pool.
	\~french
	\brief		Implàmentation d'un thread de travail à placer dans un pool de threads.
	*/
	class WorkerThread
	{
	public:
		using Job = std::function< void() >;
		using OnEnded = ashes::Signal< std::function< void( WorkerThread & ) > >;

	public:
		/**
		 *\~english
		 *\brief		Constructor.
		 *\~french
		 *\brief		Constructeur.
		 */
		WorkerThread();
		/**
		 *\~english
		 *\brief		Destructor.
		 *\~french
		 *\brief		Destructeur.
		 */
		~WorkerThread()noexcept;
		/**
		 *\~english
		 *\brief		Runs the given job.
		 *\param[in]	p_job	The job.
		 *\~french
		 *\brief		Lance la tâche donnée.
		 *\param[in]	p_job	La tâche.
		 */
		void feed( Job p_job );
		/**
		 *\~english
		 *\return		\p true if the job is ended.
		 *\~french
		 *\return		\p true si la tâche est terminàe.
		 */
		bool isEnded()const;
		/**
		 *\~english
		 *\brief		Waits for the job end for a given time.
		 *\param[in]	p_timeout	The maximum time to wait.
		 *\return		\p true if the task is ended.
		 *\~french
		 *\brief		Attend la fin de la tâche pour un temps donné.
		 *\param[in]	p_timeout	Le temps maximal à attendre.
		 *\return		\p true si la tâche est terminée.
		 */
		bool wait( utils::Milliseconds const & p_timeout )const;
		/**
		 *\~english
		 *\return		The signal raised when the worker has ended his job.
		 *\~french
		 *\return		Le signal lancé quand le worker a fini sa tâche.
		 */
		OnEnded onEnded;

	private:
		/**
		 *\~english
		 *\return		The thread loop.
		 *\~french
		 *\return		La boucle du thread.
		 */
		void doRun();

	private:
		std::unique_ptr< std::thread > m_thread;
		std::mutex m_mutex;
		std::atomic_bool m_start{ false };
		std::atomic_bool m_terminate{ false };
		Job m_currentJob;
		
	};
}

#endif