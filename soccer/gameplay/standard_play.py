import play
import ui.main
import tactics
import main

## @brief A standardized play that handles actions that an average play needs
# Right now, this is only used to implement a standard way to run or not run
# the play with defense, but any action that a normal play should do can be 
# placed here
class StandardPlay(play.Play):

    #Performs actions that all "Standard Plays" should do on initialization
    #Note: This method is called many times during the duration of a play,
    #Not just on selection
    def __init__(self, continuous):
        super().__init__(continuous)
        self.use_standard_defense()

        #If the "Use Defense" checkbox is checked and the play isn't already running
        #defense, then it adds the defense behavior. If the box isn't checked and the
        #play is running defense then it removes the behavior. Also note: it ignores
        #the requirement for goalie if the box is checked.


    _situationList = list()

    def use_standard_defense(self):
        if ui.main.defenseEnabled() and not self.has_subbehavior_with_name(
                'defense'):
            self.add_subbehavior(tactics.defense.Defense(),
                                 'defense',
                                 required=False)
        elif not ui.main.defenseEnabled():
            if self.has_subbehavior_with_name('defense'):
                self.remove_subbehavior('defense')

    #Handles activity while the play is active. A play wishing to utilize this
    #method in additionto having an "execute_running" method of its own must call
    #it via super
    def execute_running(self):
        self.use_standard_defense()

    #Since the standard_play handles defense, it will always handle the goalie
    @classmethod
    def handles_goalie(cls):
        return True


    #Since only the root play can be preempted, we can call a function for it
    #If you want to have more complex logic
    #Returns true if the preempt is sucessful
    def try_preempt(self):
        self.terminate()
        return True



    #def is_situation_valid(self, situation):

    @classmethod
    def score(cls):
        #Check to see if situation analysis is active, use default if it is not
        if (not main.situationAnalysis.enabled):
            return None
        else:
            if (main.situationAnalysis.isSituations(cls._situationList)):
                return main.situationAnalysis.inSituationScore
            else:
                return main.situationAnalysis.outSituationScore
