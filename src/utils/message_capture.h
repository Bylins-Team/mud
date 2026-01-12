#ifndef IDENTIFY_EXPORT_MESSAGE_CAPTURE_H
#define IDENTIFY_EXPORT_MESSAGE_CAPTURE_H

#include <string>
#include <sstream>

// п п╩п╟я│я│ п╢п╩я▐ п©п╣я─п╣я┘п╡п╟я┌п╟ п╡я▀п╡п╬п╢п╟ SendMsgToChar
class MessageCapture
{
public:
	MessageCapture();
	~MessageCapture();

	// п²п╟я┤п╟я┌я▄ п╥п╟я┘п╡п╟я┌ я│п╬п╬п╠я┴п╣п╫п╦п╧
	void BeginCapture();

	// п≈п╟п╨п╬п╫я┤п╦я┌я▄ п╥п╟я┘п╡п╟я┌ п╦ п╡п╣я─п╫я┐я┌я▄ п╫п╟п╨п╬п©п╩п╣п╫п╫я▀п╧ я┌п╣п╨я│я┌
	std::string EndCapture();

	// п■п╬п╠п╟п╡п╦я┌я▄ я│п╬п╬п╠я┴п╣п╫п╦п╣ п╡ п╠я┐я└п╣я─
	void AppendMessage(const char* msg);
	void AppendMessage(const std::string& msg);

	// п÷п╬п╩я┐я┤п╦я┌я▄ я┌п╣п╨я┐я┴п╦п╧ я█п╨п╥п╣п╪п©п╩я▐я─ п╥п╟я┘п╡п╟я┌я┤п╦п╨п╟ (п╢п╩я▐ пЁп╩п╬п╠п╟п╩я▄п╫п╬пЁп╬ п╢п╬я│я┌я┐п©п╟)
	static MessageCapture* GetCurrent();
	static void SetCurrent(MessageCapture* capture);

private:
	std::stringstream buffer_;
	bool capturing_;
	static thread_local MessageCapture* current_;
};

#endif // IDENTIFY_EXPORT_MESSAGE_CAPTURE_H

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
