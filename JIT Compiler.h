/*
This code is a simple example of a program that can encode x86 instructions and run them on runtime (Just in time compiler technically).
I tried to implement a simple parser for math expressions but doesn't completely work.
Code below works (I think) and generates x86 instructions for a bunch of simple math operations and puts them in virtual memory space
of function which then gets called with one of the parameters being a pointer so we can retrieve result(s)
*/

//Mit License
//Copyright © 2020 Nicolas Ortiz

#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <utility>
typedef unsigned char byte;

//Everything is written around VirtualAllocEx function (see down below) this functions can allow us to allocate data onto virtual memory and write/read onto/from it. 
//Then we can cast a pointer to it into a function ptr and call it like a function. 

/* By inserting x86 machine code into the virtual address space of a function in Windows 
we can use the function to cause the CPU to run whatever instructions we want (to some extent). 
This file contains a WIP program that compiles into machine code which is inserted
into the adress space of a function (using Win32 api) and then the function is called,
running the code. This allows us to compile and run machine code on realtime hence JIT.
It has a bunch of enums and methods for encoding x86-64 which also work for standalones (with a bit more work)
and it also has a pointer to the virtual adress space of the function which can then be called.
In its current state it is able to compile programs with math expressions but cmp loops don't work.*/

class JITEmitter
{

public:
	bool init ()
	{
		buff = (byte*)VirtualAllocEx (GetCurrentProcess (), 0, 1 << 16, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		p = buff;
		if (buff == 0)
			return false;

		return true;
	}

	enum mod
	{
		IndirectSIBNoDispDispOnly = 0b00,
		OneByteSignedDisp = 0b01,
		FourByteSignedDisp = 0b10,
		RegAdressing = 0b11
	};

	enum reg
	{
		al = 0b000,
		cl = 0b001,
		dl = 0b010,
		bl = 0b011,
		ah = 0b100,
		ch = 0b101,
		dh = 0b110,
		bh = 0b111,

		ax = 0b000,
		cx = 0b001,
		dx = 0b010,
		bx = 0b011,
		sp = 0b100,
		bp = 0b101,
		si = 0b110,
		di = 0b111,

		eax = 0b000,
		ecx = 0b001,
		edx = 0b010,
		ebx = 0b011,
		esp = 0b100,
		ebp = 0b101,
		esi = 0b110,
		edi = 0b111
	};

	enum op_modregrm
	{
		imul = 0xF7,
		idiv = 0xF6,
		xor_ = 0x33,
		cmp = 0x3B,
		add = 0x3,
		sub = 0x2b,
		jmp = 0xff
	};

	enum ext_modregrm
	{
		imul_ext = 5,
		idiv_ext = 7,
		xor_ext = 0,
		cmp_ext = 0,
		add_ext = 0,
		sub_ext = 0,
		jmp_ext = 4
	};

	enum op_reg
	{
		swap_reg_eax = 0x90,
		push_reg = 0x50,
		pop_reg = 0x58
	};

	enum op_ptr
	{
		move_from_eax = 0xA3,
		move_to_eax = 0xA1
	};

	enum op
	{
		swap_edx_eax = 0x92
	};

	void emit_instr_modregrm (uint8_t op, uint8_t ext, reg src, reg dst)
	{
		*p++ = op;
		*p++ = (RegAdressing & 0x7) << 6 | ((src + ext) & 0x7) << 3 | dst & 0x7;
	}

	void emit_instr_modregrm (op_modregrm op, ext_modregrm ext, reg src, reg dst)
	{
		*p++ = op;
		*p++ = (RegAdressing & 0x7) << 6 | ((src + ext) & 0x7) << 3 | dst & 0x7;
	}

	void emit_instr_modregrm (op_modregrm op, ext_modregrm ext, reg src)
	{
		*p++ = op;
		*p++ = (RegAdressing & 0x7) << 6 | ((src + ext) & 0x7) << 3;
	}

	void emit_instr_reg (op_reg op, reg r)
	{
		*p++ = op + r;
	}

	void emit_instr_ptr (op_ptr op, void* ptr, size_t sz_ptr)
	{
		*p++ = op;
		(void*&)p[0] = ptr; p += sz_ptr;
	}

	void emit (byte b)
	{
		*p++ = b;
	}

	void pre ()
	{
		*p++ = 0x50; // push eax
		*p++ = 0x52; // push edx
	}

	void post ()
	{
		*p++ = 0x5A; // pop edx
		*p++ = 0x58; // pop eax
		*p++ = 0xC3; // ret
	}

	void execute ()
	{
		//cast buff into func ptr and call it
		union funcptr {
			void(*x)();
			byte* y;
		} func;

		func.y = buff;
		func.x ();
	}

	byte* buff;
	byte* p;
private:
};

class JITCompiler
{
private:
	typedef long number;
	std::string::const_iterator current_char;
	JITEmitter emitter;
	std::string program;
	std::vector<number> data;
	std::unordered_map<std::string, number> labels;
	number test;

public:
	JITCompiler (const std::string& program)
		: program (program)
	{
		data.push_back (0);
	}

	number scan ()
	{
		if (!emitter.init ())
			throw std::string ("Could not initialize x86-64 emitter");
		current_char = program.begin ();

		number result = 0;
		emitter.pre ();
		scan_stmt ();

		emitter.emit_instr_reg (emitter.pop_reg, emitter.eax);
		emitter.emit_instr_ptr (emitter.move_from_eax, &result, sizeof (number*));
		emitter.post ();
		emitter.execute ();
		return result;
	}

private:

	void next_char ()
	{
		if (current_char >= program.end () || *current_char == '\0')
			throw std::string ("Unexpected EOF");
		current_char++;
	}

	void remove_ws ()
	{
		while (*current_char == ' ' || *current_char == '\t' || *current_char == '\n')
			next_char ();
	}

	bool is_digit ()
	{
		return *current_char <= '9' && *current_char >= '0';
	}

	void scan_literal ()
	{
		number acc = 0;
		while (is_digit ())
		{
			acc = (acc * 10) + (*current_char - '0');
			next_char ();
		}

		std::cout << "[literal '" << acc << "']" << std::endl;
		data.push_back (acc);
		emitter.emit_instr_ptr (emitter.move_to_eax, &data[data.size () - 1], sizeof (number*));
		emitter.emit_instr_reg (emitter.push_reg, emitter.eax);
	}

	bool is_identifier ()
	{
		return (*current_char >= 'a' && *current_char <= 'z') || (*current_char >= 'A' && *current_char <= 'Z') || *current_char == '_';
	}

	std::pair<std::string, std::string> collect_identifier ()
	{
		std::pair<std::string, std::string> res = { "", "" };

		while (is_identifier () || is_digit ())
		{
			res.first += *current_char;
			res.second += toupper (*current_char);

			next_char ();
		}
		return res;
	}

	void scan_expr ()
	{
		remove_ws ();

		if (!is_digit ())
			return;

		scan_literal ();

		remove_ws ();

		if (*current_char != '+' && *current_char != '-')
			return;

		auto op = *current_char;
		next_char ();

		remove_ws ();

		if (!is_digit ())
			throw std::string ("Expected literal after operator");

		scan_expr ();

		remove_ws ();

		std::cout << "[op '" << op << "']" << std::endl;

		if (op == '+')
		{
			emitter.emit_instr_reg (emitter.pop_reg, emitter.edx);
			emitter.emit_instr_reg (emitter.pop_reg, emitter.eax);
			emitter.emit_instr_modregrm (emitter.add, emitter.add_ext, emitter.eax, emitter.edx);
			emitter.emit_instr_reg (emitter.push_reg, emitter.eax);
		}
		else if (op == '-')
		{
			emitter.emit_instr_reg (emitter.pop_reg, emitter.edx);
			emitter.emit_instr_reg (emitter.pop_reg, emitter.eax);
			emitter.emit_instr_modregrm (emitter.sub, emitter.sub_ext, emitter.eax, emitter.edx);
			emitter.emit_instr_reg (emitter.push_reg, emitter.eax);
		}
		else
			throw std::string ("Unimplemented operand ' ") + op + "' in expression";
	}

	void scan_stmt ()
	{
		while (*current_char != '\0' && (current_char < program.end ()))
		{
			remove_ws ();
			if (is_identifier ())
			{
				auto identifier = collect_identifier ();

				if (identifier.second == "GOTO")
				{
					remove_ws ();

					if (!is_identifier ())
						throw std::string ("Expected label to go to");

					auto label = collect_identifier ().first;
					remove_ws ();

					auto found = labels.find (label);
					if (found == labels.end ())
						throw std::string ("Undefined label '") + label + "'";

					emitter.emit_instr_ptr (emitter.move_to_eax, &labels[label], sizeof (number*));
					emitter.emit_instr_modregrm (emitter.jmp, emitter.jmp_ext, emitter.eax);
				}
				else if (identifier.second == "LABEL")
				{
					remove_ws ();

					if (!is_identifier ())
						throw std::string ("Expected label for definition");

					auto label = collect_identifier ().first;
					std::cout << "[label '" << label << "': " << (uintptr_t)emitter.p << "]" << std::endl;
					remove_ws ();

					if (labels.find (label) != labels.end ())
						throw std::string ("Already defined label '") + label + "'";


					labels[label] = (intptr_t)emitter.p;
				}
				else if (identifier.second == "clear")
					emitter.emit_instr_modregrm (emitter.xor_, emitter.xor_ext, emitter.eax, emitter.eax);

				else if (identifier.second == "JZ")
				{
					remove_ws ();

					if (!is_identifier ())
						throw std::string ("Expected label to go to");

					auto label = collect_identifier ().first;
					remove_ws ();

					auto found = labels.find (label);
					if (found == labels.end ())
						throw std::string ("Undefined label '") + label + "'";

					emitter.emit_instr_reg (emitter.pop_reg, emitter.edx);
					emitter.emit_instr_ptr (emitter.move_to_eax, &data[0], sizeof (number*)); //0
					emitter.emit_instr_modregrm (emitter.cmp, emitter.cmp_ext, emitter.eax, emitter.edx);
					auto offset = (uintptr_t)emitter.p - labels[label];
					std::cout << "jrel " << offset << std::endl;
					data.push_back (offset);
					emitter.emit_instr_ptr (emitter.move_to_eax, &data[data.size()-1], sizeof (number*));

					emitter.emit_instr_modregrm (0x0F, 0x8c, emitter.eax, emitter.eax); //TODO: FIGURE OUT WHAT ADDRESS TO JUMP TO UNK

				}
				else
					throw std::string ("Unknown keyword '" + identifier.second + "'");
			}
			else
				scan_expr ();

			remove_ws ();
			if (*current_char != ';')
				throw std::string ("Expected ';'");
			next_char ();
		}
	}

};

int main ()
{
	/*JITEmitter emitter;
	if (!emitter.init ())
		return -1;
	int arg1;
	int arg2;
	int res1;
	int arg3;
	emitter.pre ();
	emitter.emit_instr_ptr (emitter.move_to_eax, &arg3, sizeof (int*));
	emitter.emit_instr_reg (emitter.push_reg, emitter.eax);
	emitter.emit_instr_ptr (emitter.move_to_eax, &arg2, sizeof (int*));
	emitter.emit_instr_reg (emitter.push_reg, emitter.eax);
	emitter.emit_instr_ptr (emitter.move_to_eax, &arg1, sizeof (int*));
	emitter.emit_instr_reg (emitter.push_reg, emitter.eax);
	emitter.emit_instr_reg (emitter.pop_reg, emitter.eax);
	emitter.emit_instr_reg (emitter.pop_reg, emitter.edx);
	emitter.emit_instr_modregrm (emitter.imul, emitter.imul_ext, emitter.eax, emitter.edx);
	emitter.emit_instr_reg (emitter.pop_reg, emitter.edx);
	emitter.emit_instr_modregrm (emitter.idiv, emitter.idiv_ext, emitter.eax, emitter.edx);*/

	//emitter.emit_instr_ptr (emitter.move_to_eax, &arg2, sizeof (int*));
	//emitter.emit_instr_reg (emitter.push_reg, emitter.eax);

	//emitter.emit_instr_ptr (emitter.move_to_eax, &arg1, sizeof (int*));

	//emitter.emit_instr_reg (emitter.swap_reg_eax, emitter.edx);
	//emitter.emit_instr_ptr (emitter.move_to_eax, &arg2, sizeof (int*)); 
	//emitter.emit_instr_modregrm (emitter.imul, emitter.imul_ext, emitter.eax, emitter.edx);
	//emitter.emit_instr_reg (emitter.swap_reg_eax, emitter.edx);

	//emitter.emit_instr_ptr (emitter.move_to_eax, &arg3, sizeof (int*));
	//emitter.emit_instr_reg (emitter.swap_reg_eax, emitter.edx);
	//emitter.emit_instr_modregrm (emitter.idiv, emitter.idiv_ext, emitter.eax, emitter.edx);
	//http://ref.x86asm.net/#column_flds
	//http://www.c-jump.com/CIS77/reference/ISA/index.html
	//http://ref.x86asm.net/coder64.html#x0FA1
	//https://gist.github.com/mikesmullin/6259449
	//http://www.c-jump.com/CIS77/CPU/x86/lecture.html#X77_0140_encoding_add_ecx_eax
	//http://www.c-jump.com/CIS77/CPU/x86/lecture.html


	//emitter.emit_instr_reg (emitter.pop_reg, emitter.eax);


	//emitter.emit_instr_ptr (emitter.move_from_eax, &res1, sizeof (long*));
	//emitter.post ();

	//arg1 = 8; arg2 = 8; arg3 = 2; res1 = 0;
	//emitter.execute ();

	//printf ("=%i\n", res1);

	JITCompiler tester("label test; 0; jz test;");

	std::cout << "x = " << tester.scan () << std::endl;
}
