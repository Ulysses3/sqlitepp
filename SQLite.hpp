#pragma once

#include "Handle.hpp"
#include "sqlite3.h"
#include <string>

/*
 * Asserting in Linux and Windows is very different.
 * Consider using the pre-processor to change the definitions
 * depending OS.
 * TODO
 */ 
#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
#define assert(condition) /*implementation defined*/
#endif

struct Exception
{
	int Result = 0;
	std::string Message;

	explicit Exception(sqlite3 *  const connection) :
		Result(sqlite3_extended_errcode(connection)),
		Message(sqlite3_errmsg(connection))
	{}
};

class Connection
{
	struct ConnectionHandleTraits : HandleTraits<sqlite3 *>
	{
		static void Close(Type value) noexcept
		{
			assert(SQLITE_OK ==  sqlite3_close(value));
		}
	};


	using ConnectionHandle = Handle<ConnectionHandleTraits>;
	ConnectionHandle m_handle;

	template <typename F, typename C>
	void InternalOpen(F open, C const * const filename)
	{
		Connection temp;
		
		if (SQLITE_OK != open(filename, temp.m_handle.Set()))
		{
			temp.ThrowLastError();
		}

		swap(m_handle, temp.m_handle);
	}

	public:
	
		Connection() noexcept = default;
		
		template <typename C>
		explicit Connection(C const * const filename)
		{
			Open(filename);
		}
		
		static Connection Memory()
		{
			return Connection(":memory:");
		}		

		static Connection WideMemory()
		{
			return Connection(L":memory:");
		}

		explicit operator bool() const noexcept
		{
			return static_cast<bool>(m_handle);
		}

		/*
 		 * ABI -> Application Binary Interface
		 */
		 sqlite3 * GetAbi() const noexcept
		 {
		 	return m_handle.Get();
		 }
	         
		/*
 		 * TODO 
 		 * this function is used in Microsoft to avoid conflicts with future extensions 	
		 * __declspec(noreturn) void ThrowLastError() const
		 * {
		 *	throw Exception(GetAbi());
		 * }
		 *
		 * If this library is to be used in Windows, comment out the above.
		 * In fact consider making a set of macros to make sure that if the library is 
		 * being built in Windows to enable the proper function.
		 */
	        [[ noreturn ]] void ThrowLastError() const
		{
			throw Exception(GetAbi());
		}

		void Open(char const * const filename)
		{
			InternalOpen(sqlite3_open, filename);			
		}
		
		/*
 		 * TODO 
 		 * I keep this Open function here for good practice.
 		 * The Linux kernel supports UTF-8 encoding for 
 		 * the filesystem thus there is no much use for it here
 		 * But Windows systems do support it so here it is.
 		 */ 
		void Open(wchar_t const * const filename)
		{
			InternalOpen(sqlite3_open16, filename);
		}	
};

template <typename T>
struct Reader
{
	int GetInt(int const column = 0) const noexcept
	{
		return sqlite3_column_int(static_cast<T const *>(this)->GetAbi(), column);
	}
	
	char const * GetString(int const column = 0) const noexcept
	{
		return reinterpret_cast<char const *>(
				sqlite3_column_text(
					static_cast<T const *>(this)->GetAbi(),
					column));
	}

	wchar_t const * GetWideString(int const column = 0) const noexcept
	{
		return static_cast<wchar_t const *>(
				sqlite3_column_text16(
					static_cast<T const *>(this)->GetAbi(),
					column));
	}

	int GetStringLength(int const column = 0) const noexcept
	{
		return sqlite3_column_bytes(static_cast<T const *>(this)->GetAbi(), column);
	}

	int GetWideStringLength(int const column = 0) const noexcept
	{
		return sqlite3_column_bytes16(static_cast<T const *>(this)->GetAbi(), column) / sizeof(wchar_t);
	}
}; 

class Statement : public Reader<Statement>
{
	struct StatementHandleTraits : HandleTraits<sqlite3_stmt *>
	{
		static void Close(Type value) noexcept
		{
			assert(SQLITE_OK == sqlite3_finalize(value));
		}	
	};

	using StatementHandle = Handle<StatementHandleTraits>;
	StatementHandle m_handle;

	template <typename F, typename C>
	void InternalPrepare(Connection const & connection, F prepare, C const * const text)
	{
		assert(connection);

		if (SQLITE_OK != prepare(connection.GetAbi(), text, -1, m_handle.Set(), nullptr))
		{
			connection.ThrowLastError();
		}
	}

	public:
		
		Statement() noexcept = default;

		explicit operator bool() const noexcept
		{
			return static_cast<bool>(m_handle);
		}

		sqlite3_stmt * GetAbi() const noexcept
		{
			return m_handle.Get();
		}

		/*
 		 * TODO
 		 *
 		 * __declspec(noreturn) void ThrowLastError() const
 		 * {
 		 * 	throw Exception(sqlite3_db_handle(GetAbi()));
 		 * }
 		 *
 		 */
		[[ noreturn ]] void ThrowLastError() const
		{
			throw Exception(sqlite3_db_handle(GetAbi()));
		}   

		void Prepare(Connection const & connection, char const * const text)
		{
			InternalPrepare(connection, sqlite3_prepare_v2, text);
		}

		void Prepare(Connection const & connection, wchar_t const * const text)
		{
			InternalPrepare(connection, sqlite3_prepare16_v2, text);
		}

		bool Step() const
		{
			int const result = sqlite3_step(GetAbi());
			
			if (result == SQLITE_ROW) return true;
			if (result == SQLITE_DONE) return false;
			
			/*
 			 * Otherwise throw an error.
 			 * It is possible to have other recoverable states.
 			 * For instance, in a multi-threaded application
 			 * a busy state can be returned.
 			 * This case and others are app-specific and must
 			 * be considered as improvements to this library on a
 			 * per-app basis.
 			 */
			ThrowLastError();  
		}

		void Execute() const
		{
			assert(!Step());
		}
};

