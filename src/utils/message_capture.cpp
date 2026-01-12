#include "message_capture.h"

// п⌠п╩п╬п╠п╟п╩я▄п╫я▀п╧ thread-local я┐п╨п╟п╥п╟я┌п╣п╩я▄ п╫п╟ я┌п╣п╨я┐я┴п╦п╧ п╥п╟я┘п╡п╟я┌я┤п╦п╨
thread_local MessageCapture* MessageCapture::current_ = nullptr;

MessageCapture::MessageCapture()
	: capturing_(false)
{
}

MessageCapture::~MessageCapture()
{
	// п∙я│п╩п╦ я█я┌п╬ я┌п╣п╨я┐я┴п╦п╧ п╥п╟я┘п╡п╟я┌я┤п╦п╨, я│п╠я─п╬я│п╦я┌я▄ пЁп╩п╬п╠п╟п╩я▄п╫я▀п╧ я┐п╨п╟п╥п╟я┌п╣п╩я▄
	if (current_ == this)
	{
		current_ = nullptr;
	}
}

void MessageCapture::BeginCapture()
{
	buffer_.str("");
	buffer_.clear();
	capturing_ = true;
	current_ = this;
}

std::string MessageCapture::EndCapture()
{
	capturing_ = false;
	current_ = nullptr;
	return buffer_.str();
}

void MessageCapture::AppendMessage(const char* msg)
{
	if (capturing_ && msg)
	{
		buffer_ << msg;
	}
}

void MessageCapture::AppendMessage(const std::string& msg)
{
	if (capturing_)
	{
		buffer_ << msg;
	}
}

MessageCapture* MessageCapture::GetCurrent()
{
	return current_;
}

void MessageCapture::SetCurrent(MessageCapture* capture)
{
	current_ = capture;
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
