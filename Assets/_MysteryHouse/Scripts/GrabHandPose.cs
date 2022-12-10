using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Interaction.Toolkit;

#if UNITY_EDITOR
using UnityEditor;
#endif

public class GrabHandPose : MonoBehaviour
{
    [SerializeField]
    private float poseTransitionDuration = 0.2f;
    [SerializeField]
    private HandData rightHandPose;
    [SerializeField]
    private HandData leftHandPose;

    private Vector3 startingHandPosition;
    private Vector3 finalHandPosition;
    private Quaternion startingHandRotation;
    private Quaternion finalHandRotation;

    private Quaternion[] startingFingerRotations;
    private Quaternion[] finalFingerRotations;
    
    void Start()
    {
        PixartXRGrabInteractable xrGrabInteractable = gameObject.GetComponent<PixartXRGrabInteractable>();
        xrGrabInteractable.selectEntered.AddListener(SetupPose);
        xrGrabInteractable.selectExited.AddListener(UnsetPose);
        rightHandPose.gameObject.SetActive(false);
        leftHandPose.gameObject.SetActive(false);
    }

    public void SetupPose(BaseInteractionEventArgs arg)
    {
        if (arg.interactorObject != null)
        {
            HandData handData = arg.interactorObject.transform.GetComponentInChildren<HandData>();
            handData.Animator.enabled = false;

            if (handData.HandType == HandType.Right)
            {
                SetHandDataValues(handData, rightHandPose);
            }
            else
            {
                SetHandDataValues(handData, leftHandPose);
            }
            
            //SendHandData(handData, finalHandPosition, finalHandRotation, finalFingerRotations);
            StartCoroutine(SetHandDataRoutine(handData, finalHandPosition, finalHandRotation, finalFingerRotations, startingHandPosition, startingHandRotation, startingFingerRotations));
        }
    }

    public void SetHandDataValues(HandData h1, HandData h2)
    {
        //startingHandPosition = h1.Root.localPosition;
        startingHandPosition = new Vector3(h1.Root.localPosition.x / h1.Root.localScale.x, h1.Root.localPosition.y / h1.Root.localScale.y, h1.Root.localPosition.z / h1.Root.localScale.z);
        //finalHandPosition = h2.Root.localPosition;
        finalHandPosition = new Vector3(h2.Root.localPosition.x / h2.Root.localScale.x, h2.Root.localPosition.y / h2.Root.localScale.y, h2.Root.localPosition.z / h2.Root.localScale.z);

        startingHandRotation = h1.Root.localRotation;
        finalHandRotation = h2.Root.localRotation;

        startingFingerRotations = new Quaternion[h1.FingerBones.Length];
        finalFingerRotations = new Quaternion[h1.FingerBones.Length];

        for (int i = 0; i < h1.FingerBones.Length; i++)
        {
            startingFingerRotations[i] = h1.FingerBones[i].localRotation;
            finalFingerRotations[i] = h2.FingerBones[i].localRotation;
        }
    }

    //public void SendHandData(HandData h, Vector3 newPosition, Quaternion newRotation, Quaternion[] newBonesRotation)
    //{
    //    h.Root.localPosition = newPosition;
    //    h.Root.localRotation = newRotation;

    //    for (int i = 0; i < newBonesRotation.Length; i++)
    //    {
    //        h.FingerBones[i].localRotation= newBonesRotation[i];
    //    }
    //}

    public void UnsetPose(BaseInteractionEventArgs arg)
    {
        if (arg.interactorObject != null)
        {
            HandData handData = arg.interactorObject.transform.GetComponentInChildren<HandData>();
            handData.Animator.enabled = true;

            //SendHandData(handData, startingHandPosition, startingHandRotation, startingFingerRotations);
            StartCoroutine(SetHandDataRoutine(handData, startingHandPosition, startingHandRotation, startingFingerRotations, finalHandPosition, finalHandRotation, finalFingerRotations));
        }
    }

    public IEnumerator SetHandDataRoutine(HandData h, Vector3 newPosition, Quaternion newRotation, Quaternion[] newBonesRotation, Vector3 startingPosition, Quaternion startingRotation, Quaternion[] startingBoneRotation)
    {
        float timer = 0;

        while (timer < poseTransitionDuration)
        {
            float timerDivide = timer / poseTransitionDuration;

            Vector3 p = Vector3.Lerp(startingPosition, newPosition, timerDivide);
            Quaternion r = Quaternion.Lerp(startingRotation, newRotation, timerDivide);

            h.Root.localPosition = p;
            h.Root.localRotation = r;

            for (int i = 0; i < newBonesRotation.Length; i++)
            {
                h.FingerBones[i].localRotation = Quaternion.Lerp(startingBoneRotation[i], newBonesRotation[i], timerDivide);
            }

            timer += Time.deltaTime;
            yield return null;
        }
    }

#if UNITY_EDITOR

    [MenuItem("Tools/Mirror Selected Right Grab Pose")]
    public static void MirrorRightPose()
    {
        Debug.Log("Mirror Right Pose");
        GrabHandPose handPose = Selection.activeGameObject.GetComponent<GrabHandPose>();
        handPose.MirrorPose(handPose.leftHandPose, handPose.rightHandPose);
    }
#endif

    public void MirrorPose(HandData poseToMirror, HandData poseUsedToMirror)
    {
        Vector3 mirroredPosition = poseUsedToMirror.Root.localPosition;
        mirroredPosition.x *= -1;

        Quaternion mirroredQuaternion = poseUsedToMirror.Root.localRotation;
        mirroredQuaternion.y *= -1;
        mirroredQuaternion.z *= -1;

        poseToMirror.Root.localPosition = mirroredPosition;
        poseToMirror.Root.localRotation = mirroredQuaternion;

        for (int i = 0; i < poseUsedToMirror.FingerBones.Length; i++)
        {
            poseToMirror.FingerBones[i].localRotation = poseUsedToMirror.FingerBones[i].localRotation;
        }
    }

}
