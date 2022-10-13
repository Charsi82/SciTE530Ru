// TWL_DATA.H
#pragma once
#include "twl_cntrls.h"

class EBadNumber
{
	int m_why;
};

class Converter {
protected:
	int m_digits;
	TControl* m_ctrl;
public:
	Converter() : m_digits(4), m_ctrl(nullptr) {};
	void set_ctrl(TControl* ctrl) { m_ctrl = ctrl; }
	virtual wchar_t* get_str() { return (wchar_t*)L""; }
	virtual void set_str(pchar) { };
	virtual double get_double();
	virtual void set_double(double);
	virtual int get_long();
	virtual void set_long(int);
};

class EditConverter : public Converter {
public:
	// overrides:
	wchar_t* get_str();
	void set_str(pchar);
	TEdit* edit() { return (TEdit*)m_ctrl; }
};

class CheckboxConverter : public Converter {
public:
	// overrides:
	int get_long();
	void set_long(int);
	TCheckBox* check_box() { return (TCheckBox*)m_ctrl; }
};

class ListboxConverter : public Converter {
public:
	// overrides:
	wchar_t* get_str();
	void set_str(pchar);
	TListBox* list_box() { return (TListBox*)m_ctrl; }
};

class Data {
protected:
	Converter* m_co;
	void* m_ptr;
public:
	Data(void* ptr) : m_co(nullptr), m_ptr(ptr) { }
	void converter(Converter* co) { m_co = co; }
	virtual void write() = 0;
	virtual void read() = 0;
};

typedef Data* PData;

class StringData : public Data {
public:
	StringData(void* ptr) : Data(ptr) { }
	void write();
	void read();
};

class FloatData : public Data {
public:
	FloatData(void* ptr) : Data(ptr) { }
	void write();
	void read();
};

class DoubleData : public Data {
public:
	DoubleData(void* ptr) : Data(ptr) { }
	void write();
	void read();
};

class IntData : public Data {
public:
	IntData(void* ptr) : Data(ptr) { }
	void write();
	void read();
};

class BoolData : public Data {
public:
	BoolData(void* ptr) : Data(ptr) { }
	void write();
	void read();
};

class ShortData : public Data {
public:
	ShortData(void* ptr) : Data(ptr) { }
	void write();
	void read();
};

class UIntData : public Data {
public:
	UIntData(void* ptr) : Data(ptr) { }
	void write();
	void read();
};
