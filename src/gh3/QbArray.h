#pragma once

#include "core\GH3Plus.h"

#include "QbValueType.h"
#include <stdint.h>

namespace GH3
{

	struct GH3P_API QbArray
	{

	private:
		QbValueType m_type;
		uint32_t m_size;
		uint32_t *m_arr;

	public:
		int * __thiscall Initialize(int size, int type);

		void Clear();
		void Set(int index, uint32_t value);
		uint32_t Get(int index);

		
		void SetType(QbValueType type);
		inline QbValueType Type() { return m_type;}
		inline uint32_t Length() { return m_size;}
	};

}