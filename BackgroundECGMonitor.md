
# History: This project originated from a health crisis

Personally, this design came about because of a very serious health crisis that I went through, namely a heart attack. In April, 2024, I was moving some extremely heavy bags of soil for my garden, when I felt tremendous pressure in both shoulder, and I grew very fatigued. I wound up first in the Emergency Room at Skokie Hospital, where the medical team first stabilized me, and prepared me for transport to Evanston Hospital. Arriving at Evanston Hospital, I was immediately sent to their Cardiac Cath Lab, where I underwent a Cardiac Catheterization procedure, and two stents were inserted into the cardiac arteries. There was more than 90% blockage. I received excellent from the Emergency care team at Skokie Hospital, and then from the Cardiac Care teams at Evanston Hospital. I am forever grateful to the excellent care I received from the physicians, nurses, and the other clinicians at Endeavor Health.

This was a devastating health event for me, and as I recovered, I was determined to finish an ECG monitoring project I had started a few years prior to this crisis. This event did push me to resurrect this project.

# Why didn't I take cholesterol treatment earlier to prevent this?

I did, as a matter of fact! I had tried several statins, but each time I tried, I experienced severe muscle pain in both my arms and legs, so I ended up discontinuing the statins. In my own mind, along with a large proportion of the adult population, I blamed statins as the primary cause of the severe limb pain, and there is plenty of available documentation on may patients experiencing the limb pain, and the association that many have with statin usage (TBD: find the sources).  

After the heart attack, I was strongly advised to go back on statins, and the aches and pains became quite prevalent. Once again, I tried several statins. Finally, my Cardiologist, Dr. Harris, suggested an injectable, Praluent, generic known as Alirocumab. This was considerably more expensive than the statins. While the price was lowered by the Medicare supplemental, it nevertheless, is quite expensive. It did succeed at lowering the cholesterol.

# The pain I associated with statins was probably caused in part by spinal stenosis

In the meantime, the pain experienced in my arms, and in particular, my hands, became very intense. I was also having severe headaches again, and since I had a previous cervical fusion operation in 2016, I contacted a spinal specialist in 2025. I went to see a spinal pain clinic, and I received a spinal radiofrequency ablation procedure. It really did not help, and I went to see another pain specialist, Dr. Andrew Engel, who ordered an MRI of the cervical region. Dr. Engel's diagnosis was that the MRI indicated severe problems in the cervical spine. In January 2026, I had an anterior fusion C3, C4, performed by Dr. Musacchio, which was above the previous cervical spinal operation. 

The operation was a great success. The pains in my arms and my hands were now gone. Interestingly, I noticed an unusual set of symptoms, that I had attributed to the past statin usage. I had pains in my arms and hands that seemed completely symmetric with respect to right and left limbs. One example, was severe pain in my right index finger but also in my left index finger as well. Pains in my thumbs and other fingers were also strangely symmetrical, with respect to right and left. This I associated with pain caused by the statins. Now with the operation being a success, not only was the pain in general relieved, but also the symmetrical nature of the pain was also gone. In the back of my mind, I knew that whatever the statin molecule is, it cannot "coordinate" the experience of pain on both right and left sides. Hindsight is 20/20, as the old saying goes, and thus I realized that the statin usage was not the principle cause of the aches and pain I had experienced.

I did query Google AI, and a document was created that outlines the confusion between static pains and spinal stenosis. Please see docs/JupyterBook/Spinal Stenosis symptoms and statin-associated muscle symptoms (SAMS).odt

# How the work progressed: Microsoft Visual Studio Code. DeepSeek V4 Pro model and GitHub Copilot are the copilots for this project

Being 70ish, post-recovery heart attack, and an ongoing cataract, forced me to consider as many sensible and useful tools as possible in order to complete it in any sort of reasonable time. I was reluctant to use AI to help me, but the health issues finally convinced me to research into what was needed in order to get it installed and working. I had already been working with Microsoft Visual Studio Code, along with the Espressif ESP-IDF extension.

I also had been using several AI chat services: Google AI, ESP Docs AI Assistant, along with DeepSeek Chat. All of them are excellent assistants, and I will add documentation that shows what was discussed and suggested by the services.

I have had AI do both software and hardware work for the design and development of the ECG monitor system. At, first I had done a number of chat sessions, where I would map out some design inquiries within the chat. At first, mostly firmware issues, often with just small queries involving the ESP-IDF API (ESP-IDF is the framework created by Espressif) for some component software issues. As time progressed, I needed some help with hardware related problems that I was trying to get a better handle on. I found that the AI could show much better insight on some complex, and sometimes contradictory information stored in a datasheet for a given device. 

After intial It originally started as a rather involved debugging problem, in specific, a problem with data capture from the ADC board, the ADS1115, which is very powerful, but complex device to use.

# What are the goals for ECG Monitor?
* First and foremost, it should be a cheap, continuous ECG monitor that has the ability to digitize and log ECG data from the various leads into a recording device. 
* Using a set of signal processing functions, have the device determine basic cardiac statistics, such as beats per minute (BPM), as well as signalling any cardiac events that should be notified to not only the patient, and if it can be provided, communicated to a health professional.
* For extended period use, provide the storage and communication means for data transfer of ECG data.

# Future projects
* ECG Signal generator;  ECG Simulator teaching tool
    * This is a corollary to the ECG monitor system. This could represent a significant teaching tool for clinical students.

* Larger projects: Air Quality and Cardiovascular health impacts.
    * Summer 2026 brought very dense and unhealthy smoke over much of the MidWest. The Air Quality Index represented dangerous levels of AQI over several days, and even being indoors most of the time did not shield us form the harmful effects. See: https://doorcountyknock.org/2026/08/its-a-beautiful-day-to-stay-inside-learning-more-about-wildfire-smoke
    * Searches on AQI and Cardiovacular Health has yielded many "hits": https://jamanetwork.com/journals/jamanetworkopen/fullarticle/2817296, Public Health Relevance of US EPA Air Quality Index Activity Recommendations, JAMA Network Open, Vol 7, No. 4, April 8, 2024: Findings  This cross-sectional study modeled the number needed to treat (NNT) per day by following AQI activity guidance to prevent 1 serious atherosclerotic cardiovascular disease (ASCVD) or pulmonary event. The NNT remained high during most days (96%) when PM2.5 was elevated (AQI, 101-200) for the general adult population (>18 million), adults with ASCVD (approximately 1.6-5.0 million), and adults with pulmonary disease (approximately 66 000-202 000).

* Larger projects: LDL, Microplastics and Health
	* Proposal for possible preventative LDL lowering procedures. 
    * See: docs/JupyterBook/LDL across the globe and what can be done about it.odt



