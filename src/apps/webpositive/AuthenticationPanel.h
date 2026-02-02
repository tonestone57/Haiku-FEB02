/*
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef AuthenticationPanel_h
#define AuthenticationPanel_h

#include <String.h>
#include <Window.h>

class BCheckBox;
class BMessageRunner;
class BTextControl;

class AuthenticationPanel : public BWindow {
public:
	AuthenticationPanel(BRect parentFrame = BRect());
	virtual ~AuthenticationPanel();

	virtual bool QuitRequested();

	virtual void MessageReceived(BMessage *message);

	bool getAuthentication(const BString& text, const BString& previousUser,
		const BString& previousPass, bool previousRememberCredentials,
		bool badPassword, BString& user, BString& pass,
		bool* rememberCredentials);

private:
	BRect m_parentWindowFrame;
	BTextControl* m_usernameTextControl;
	BTextControl* m_passwordTextControl;
	BCheckBox* m_hidePasswordCheckBox;
	BCheckBox* m_rememberCredentialsCheckBox;
	BButton* m_okButton;
	BButton* m_cancelButton;

	bool m_cancelled;

	sem_id m_exitSemaphore;

	BMessageRunner* m_jitterRunner;
	BPoint m_jitterStart;
	int32 m_jitterStep;
};

#endif // AuthenticationPanel_h
