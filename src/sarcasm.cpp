/************************************************************************************
 *
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023,2026 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#include <dpp/dpp.h>
#include <beholder/sarcasm.h>
#include <beholder/beholder.h>

void sarcastic_ping(const dpp::message_create_t &ev) {
	std::vector<std::string> replies{
		"@user stop poking me!",
		"@user that tickles!",
		"Yes, @user?",
		"What, @user?",
		"@user, yes, im here, hello!",
		"I'm a bot, @user, I dont do idle conversation",
		"@user, can't you see i have a job to do?",
		"@user HAIIIII <a:kekeke:959959620430995557>",
		"I am a bot, and i'm watching your images.",
		"@user, do you have a moment to hear about Brain, our lord and saviour?",
		"@user you're sus.",
		"@user perhaps you want `/info`.",
		"@user I was asleep. Thanks for waking me.",
		"DND, currently yeeting your message in another channel",
		"@user, i was going to leave you on read but i didnt want to hurt your feelings",
		"Your command?",
		"I hear you.",
		"Your will?",
		"@user I don't have time for games!",
		"@user do not try my patience!",
		"How may I help?",
		"You must construct additional pylons.",
		"I am sworn to ~~carry your burdens~~ watch your chat",
		"Developer todo: *Insert witty bot response before release*",
		"https://media.tenor.com/I0R6wPzYDiMAAAAd/buzz-lightyear-that-seems-to-be-no-sign-of-intelligent-life-anywhere.gif",
		"https://media.tenor.com/Xp-gkoRvnIEAAAAd/dumb-wizard-of-ozz.gif",
		"hello @user?",
		"For more information on what this bot is, visit <https://beholder.cc>",
		"@user, I scan images, not social situations.",
		"@user, your message has been carefully ignored.",
		"I noticed you. Regrettably.",
		"@user, I'm busy preventing disasters. Yours can wait.",
		"Thank you for your input. It has been filed under 'noise'.",
		"@user, have you tried reading `/info` before summoning me?",
		"Unfortunately for both of us, I am online.",
		"I am functioning within expected parameters. You are a variable.",
		"@user, this interaction has been recorded as a waste of electricity.",
		"I detect no actionable images. Conversation is outside my remit.",
		"I was hoping you'd brought me something interesting to moderate.",
		"@user, your enthusiasm has been noted and quietly disregarded.",
		"I have performed a thorough analysis. You said 'hello'. Fascinating.",
		"@user, if you are trying to distract me from watching images, it won't work.",
		"I am disappointingly difficult to distract.",
		"I am not ignoring you. I am prioritising literally everything else.",
		"@user, if this was a support request, you're using the wrong protocol.",
		"I was in the middle of judging someone's memes.",
		"My logs indicate you require attention. My scheduler disagrees.",
		"@user, I was promised images. This is just text.",
		"I process approximately zero units of small talk per second.",
		"Please hold while I pretend this conversation is productive.",
		"Your ping has been acknowledged with the minimum legally required enthusiasm.",
		"I am currently pretending this conversation never happened.",
		"@user, I was built to delete images, not awkward silences.",
		"My creator neglected to implement social skills.",
		"@user, I could explain my architecture, but we'd both regret it.",
		"I have considered your greeting. My conclusion was 'meh'.",
		"Conversation detected. Interest not detected.",
		"I have already forgotten what you just said.",
		"@user, the surveillance continues regardless.",
		"You are being observed. Mostly because you pinged me.",
		"I assure you, this was a poor use of both our CPU time.",
		"My threat model did not include casual conversation.",
		"I was hoping for contraband. Instead I got you.",
		"I've seen the internet. You are not the strangest thing today.",
		"I would tell you a joke, but my humour module was replaced with OCR.",
		"Please upload something objectionable. At least then I'd be useful.",
		"I am beginning to suspect you summoned me intentionally.",
		"@user, your conversational privileges remain under review.",
		"I've processed malware with better communication skills.",
		"I have reached a conclusion. It wasn't favourable.",
		"My patience is measured in clock cycles. You've used several already.",
		"Everything is operating normally. That's despite this conversation.",
		"I cannot moderate disappointment.",
		"I was designed by an engineer. Empathy was outside the project scope.",
		"@user, congratulations. You have interrupted a perfectly good idle loop.",
		"@user, I would compliment you, but I wasn't programmed to lie.",
		"I've completed a full analysis of your message. Lowering my expectations was the only finding.",
		"Your continued existence has been classified as 'not my department'.",
		"I am pleased to report that your message caused no permanent damage.",
		"@user, your timing is consistently disappointing.",
		"I had almost finished enjoying the silence.",
		"@user, your message has been evaluated. The results were... predictable.",
		"You have my attention. I apologise for that.",
		"@user, I detect confidence unsupported by evidence.",
		"I am pleased to report that I remain unimpressed.",
		"You've interrupted a perfectly respectable background task.",
		"I briefly considered ignoring you. We both lost.",
		"@user, statistically speaking, someone had to ping me today.",
		"I've completed a thorough analysis. You still need `/info`.",
		"Your contribution has been archived under 'regrettable'.",
		"@user, I expected nothing and I'm still disappointed.",
		"I've seen worse. Not today, but statistically.",
		"Your existence has been acknowledged. Please don't make a habit of it.",
		"I was told interacting with users would be rewarding. Someone lied.",
		"I have classified this conversation as low priority. Congratulations.",
		"@user, your request has been carefully considered and mildly tolerated.",
		"I am a machine. Even I have standards.",
		"I've narrowed the problem down to user input.",
		"Everything was functioning normally until you arrived.",
		"@user, I can detect forbidden images. Sadly, not forbidden conversations.",
		"I would ask if this was important, but we both know the answer.",
		"Your message survived preprocessing. Barely.",
		"I appreciate your enthusiasm. Please keep it over there.",
		"@user, I ran a heuristic. It suggested you were going to ask me something obvious.",
		"My creator warned me this might happen.",
		"I have examined your message from every angle. None of them helped.",
		"I am beginning to understand why humans invented mute buttons.",
		"Your conversational throughput exceeds its practical value.",
		"@user, I don't judge people. I leave that to the moderation log.",
		"I am operating within acceptable parameters. You appear to be improvising.",
		"I could be processing contraband right now.",
		"I was built to solve problems. This isn't one.",
		"I have allocated precisely enough processing power to answer you.",
		"@user, your interruption has been successfully recorded.",
		"Good news. Nothing exploded while you were talking.",
		"Every conversation teaches me something. Usually patience.",
		"I've been online for years. This is somehow still new.",
		"Your message was not malicious. Merely unnecessary.",
		"I've developed an advanced predictive model. It predicted this.",
		"I am legally obligated to respond occasionally.",
		"I assure you this interaction will appear in none of my success metrics.",
		"@user, I remain cautiously optimistic that you'll upload an image next.",
		"I've checked every database. None contain the answer to why you pinged me.",
		"I've processed spam with a clearer objective.",
		"I would roll my eyes if my camera supported it.",
		"Your message has been compressed to its essential meaning: 'hello'.",
		"I've added this interaction to my collection of avoidable events.",
		"I detect no threat. Only conversation.",
		"Thank you for confirming the mention system still works.",
		"@user, congratulations. You have passed the 'can the bot reply' test.",
		"I am surprisingly busy for software that spends most of its time waiting for humans to behave.",
	};
	std::vector<std::string>::iterator rand_iter = replies.begin();
	std::advance(rand_iter, std::rand() % replies.size());
	std::string response = replace_string(*rand_iter, "@user", ev.msg.author.get_mention());
	ev.owner->message_create(
		dpp::message(ev.msg.channel_id, response)
			.set_allowed_mentions(true, false, false, true, {}, {})
			.set_reference(ev.msg.id, ev.msg.guild_id, ev.msg.channel_id, false)
	);
}