from diagrams import Diagram
from diagrams.programming.flowchart import Decision, Preparation, Display
from diagrams.aws.compute import EC2
from diagrams.aws.database import RDS
from diagrams.aws.network import ELB


with Diagram("test_run", show=False):
    Preparation("Prepare for CBT") >> (dec := Decision("Left or Right?"))
    dec >> Display("Left :(")
    dec >> Display("Right :)")
    dec >> Display("Cock >:)")
